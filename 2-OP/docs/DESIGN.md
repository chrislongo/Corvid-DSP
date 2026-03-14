# 2-OP — Implementation Design

This document translates REQUIREMENTS.md into a concrete implementation plan. Read alongside the root CLAUDE.md for toolchain and JUCE conventions.

---

## Source layout

```
2-OP/
├── CMakeLists.txt
├── CLAUDE.md
├── docs/
│   ├── REQUIREMENTS.md
│   ├── DESIGN.md           (this file)
│   └── TODO.md
├── design/
│   ├── 2-op-mockup.svg          (original 4-slider layout, deprecated)
│   └── 2-op-mockup-v2.svg       (current: 4 sliders + 4 knobs)
└── src/
    ├── PluginProcessor.h
    ├── PluginProcessor.cpp
    ├── PluginEditor.h
    └── PluginEditor.cpp
```

No additional source files. The Plaits engine files are compiled as extra sources listed in CMakeLists.txt — they do not live under `src/`.

---

## CMakeLists.txt

### Key settings

```cmake
cmake_minimum_required(VERSION 3.22)
project(TwoOpFM VERSION 0.1.0)

add_subdirectory(/Users/chris/src/github/JUCE JUCE)

set(EURORACK /Users/chris/src/github/eurorack)

juce_add_plugin(TwoOpFM
    COMPANY_NAME            "CorvidAudio"
    PLUGIN_MANUFACTURER_CODE CVDA
    PLUGIN_CODE             TWOP
    FORMATS                 AU Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    AU_EXPORT_PREFIX        TwoOpFMAU
    BUNDLE_ID               com.CorvidAudio.TwoOpFM
    PRODUCT_NAME            "2-OP"
)

juce_generate_juce_header(TwoOpFM)   # required — generates JuceHeader.h

target_sources(TwoOpFM PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    ${EURORACK}/plaits/dsp/engine/fm_engine.cc
    ${EURORACK}/plaits/resources.cc
    ${EURORACK}/stmlib/dsp/units.cc       # pitch ratio LUTs
)

target_include_directories(TwoOpFM PRIVATE ${EURORACK})

target_compile_definitions(TwoOpFM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    TEST=1    # bypasses ARM VFP inline assembly in stmlib/dsp/dsp.h (required for x86_64)
)

target_link_libraries(TwoOpFM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_plugin_client
)
```

**`TEST=1` is mandatory**: `stmlib/dsp/dsp.h` contains ARM-specific inline assembly (`vsqrt.f32`, `ssat`, `usat`) guarded by `#ifndef TEST`. Without it the build fails on x86_64.

---

## PluginProcessor

### Class: `TwoOpFMAudioProcessor`

#### Members

```cpp
// APVTS — public so editor can attach sliders/knobs
juce::AudioProcessorValueTreeState apvts;

// Plaits FM engine
plaits::FMEngine engine_;
char engine_buffer_[256];
stmlib::BufferAllocator allocator_;

float out_[plaits::kBlockSize];
float aux_[plaits::kBlockSize];
bool  already_enveloped_ = false;

// Voice state
int   sounding_note_        = 60;   // kept after note-off so LPG tail has a pitch
float velocity_             = 1.0f;
float pitch_bend_semitones_ = 0.0f;
float pitch_correction_     = 0.0f; // compensates Plaits' hardcoded a0
bool  gate_                 = false; // true while key is physically held
int   trigger_              = plaits::TRIGGER_LOW; // RISING_EDGE → HIGH → LOW

// LPG
plaits::LPGEnvelope lpg_envelope_;
plaits::LowPassGate lpg_;
float lpg_gate_level_  = 0.0f;  // current ramped level fed to ProcessLP
float lpg_gate_target_ = 0.0f;  // velocity_ on note-on, 0 on note-off

// FM smoothers — 20 ms ramp, one SmoothedValue per parameter
juce::SmoothedValue<float> ratio_smoothed_;
juce::SmoothedValue<float> index_smoothed_;
juce::SmoothedValue<float> feedback_smoothed_;
juce::SmoothedValue<float> sub_smoothed_;
```

#### APVTS layout

| ID         | Name         | Range              | Default  | Taper           |
|------------|--------------|--------------------|----------|-----------------|
| `ratio`    | "Ratio"      | 0–1                | 0.5      | Linear          |
| `index`    | "Index"      | 0–1                | 0.3      | Linear          |
| `feedback` | "Feedback"   | 0–1                | 0.5      | Linear          |
| `sub`      | "Sub"        | 0–1                | 0.0      | Linear          |
| `attack`   | "Attack"     | 0.005–2.0 s        | 0.010 s  | Skewed (0.3)    |
| `decay`    | "Decay"      | 0–1                | 0.5      | Linear          |
| `color`    | "Color"      | 0–1                | 0.0      | Linear          |
| `ping`     | "Ping Mode"  | false/true         | false    | —               |
| `output`   | "Output"     | −40–0 dB           | −12 dB   | Linear (dB)     |

FM params are smoothed (20 ms). LPG and output params are read raw once per block.

#### `prepareToPlay(sampleRate, samplesPerBlock)`

```
1. allocator_.Init(engine_buffer_, sizeof(engine_buffer_))
2. engine_.Init(&allocator_)
3. pitch_correction_ = 12.0f * log2f(47872.34f / sampleRate)
   // Plaits' a0 is hardcoded for its hardware rate (47872.34 Hz);
   // this semitone offset corrects pitch at any host sample rate.
4. lpg_envelope_.Init(); lpg_.Init()
5. lpg_gate_level_ = lpg_gate_target_ = 0.0f
6. For each SmoothedValue: reset(sampleRate, 0.02), setCurrentAndTargetValue(default)
```

#### `processBlock`

```
1. Read LPG params once: atk, decay, color, pingMode, outputGain

2. Compute Plaits decay coefficients (same formula as plaits/dsp/voice.cc):
     short_decay = (200 × kBlockSize / sr) × 2^(−96 × decay / 12)
     decay_tail  = (20  × kBlockSize / sr) × 2^((−72 × decay + 12 × color) / 12) − short_decay
   Compute attack step rate:
     atk_step = (atk > 0) ? 1.0 / (atk × blockRate) : 1.0

3. Update FM smoother targets from APVTS

4. Interleave MIDI events with rendering at exact samplePosition boundaries:
   - Note On:  sounding_note_ = note, velocity_ = vel/127,
               gate_ = true, trigger_ = TRIGGER_RISING_EDGE,
               lpg_gate_target_ = velocity_
               if pingMode: lpg_envelope_.Trigger()
   - Note Off: if note == sounding_note_:
               gate_ = false, trigger_ = TRIGGER_LOW,
               lpg_gate_target_ = 0
               (sounding_note_ kept — engine needs pitch during tail)
   - Pitch wheel: pitch_bend_semitones_ = (raw - 8192) / 8192.0f * 2.0f

5. Render each inter-event segment in kBlockSize (12-sample) chunks:
   a. Advance all four smoothers exactly chunk samples
   b. Fill params: note = sounding_note_ + pitch_bend_ + pitch_correction_
                   harmonics = ratio, timbre = index, morph = feedback
                   accent = velocity_, trigger = trigger_
   c. engine_.Render(params, out_, aux_, chunk, &already_enveloped_)
   d. Advance trigger state: RISING_EDGE → HIGH after first chunk
   e. Advance LPG envelope one block:
        if pingMode: lpg_envelope_.ProcessPing(ping_attack, short_decay, decay_tail, color)
        else:        ramp lpg_gate_level_ toward lpg_gate_target_ at atk_step
                     lpg_envelope_.ProcessLP(lpg_gate_level_, short_decay, decay_tail, color)
   f. Blend sub into out_: out_[i] = (out_[i] + sub_val × aux_[i]) × 0.6
   g. Apply LPG: lpg_.Process(gain, frequency, hf_bleed, out_, chunk)
   h. Clamp and apply outputGain; write to left + right channels

6. Skip rendering segments with no active LPG output (gate_ false and gain < 0.001)
```

`getTailLengthSeconds()` returns 4.0 (accommodates longest decay tail).

---

## PluginEditor

### Panel size

**545 × 455 px**

Two rounded-rect sections:
- **Top (FM CONTROLS)**: 4 vertical sliders, track y=95–255
- **Bottom (LPG + OUTPUT)**: 4 rotary knobs centred at y=372, PING pill at y≈307

### Column centres

```
FM sliders:  x = 83, 209, 336, 463
LPG knobs:   x = 83, 210, 337, 463
```

---

### LookAndFeel 1: `FMSliderLookAndFeel` (vertical sliders)

Subclasses `juce::LookAndFeel_V4`. Overrides `drawLinearSlider` and `getSliderThumbRadius`.

**`getSliderThumbRadius`** → returns `kThumbH / 2` (= 6). Constrains `sliderPos` to
`[thumbRadius, height - thumbRadius]`, keeping the thumb fully inside the component.

**Slider component bounds** (set in `resized()`):
```
kTrackTop  = 95,   kTrackBot = 255,   kTrackWidth = 5
kThumbW    = 42,   kThumbH   = 13
kSliderTop = kTrackTop - kThumbH/2  = 89
kSliderH   = kTrackBot - kSliderTop + kThumbH/2  = 172
kSliderW   = 44
bounds: (colX - kSliderW/2, kSliderTop, kSliderW, kSliderH)
```

**`drawLinearSlider`** (coordinates relative to slider component top):
```
localTrackTop = kTrackTop - kSliderTop  = 6
localTrackBot = kTrackBot - kSliderTop  = 166
trackH        = 160

Tick marks (10 ticks, colour 0xff888888):
  ty = localTrackTop + i × trackH / (kTickCount + 1),  i = 1..10
  left:  (cx-9, ty) → (cx-3, ty)
  right: (cx+3, ty) → (cx+9, ty)

Track (colour 0xff2a2a2a):
  fillRect(cx - kTrackWidth/2, localTrackTop, kTrackWidth, trackH)

Thumb (matte-black, colour 0xff111111):
  thumbCy = sliderPos  (provided by JUCE, clamped by getSliderThumbRadius)
  fillRect(cx - kThumbW/2, thumbCy - kThumbH/2, kThumbW, kThumbH)

Hairline (white, 1px):
  drawLine(cx - kThumbW/2, thumbCy, cx + kThumbW/2, thumbCy)
```

**Shift key fine-tune**: `mouseDown` captures a fine-tune multiplier when Shift is held, applied via `setVelocityModeParameters`.

---

### LookAndFeel 2: `ADSRKnobLookAndFeel` (rotary knobs)

Subclasses `juce::LookAndFeel_V4`. Overrides `drawRotarySlider`.

**Style**: `juce::Slider::RotaryVerticalDrag`

**Geometry**:
```
kKnobR   = 26
kKnobW   = kKnobR * 2 + 24  = 76  (extra margin for tick marks)
kKnobCY  = 372
Sweep: −135° to +135° from 12 o'clock (270° total)
  kRotaryStart = π × 1.25
  kRotaryEnd   = π × 2.75
```

**`drawRotarySlider`**:
```
cx, cy = centre of component bounds
r = kKnobR

1. Fill circle, radius r, colour 0xff111111
2. Draw tick marks at standard positions around perimeter (grey)
3. Compute indicator angle:
     angle = kRotaryStart + sliderPos × (kRotaryEnd − kRotaryStart)
4. Draw indicator line:
     x1 = cx + sin(angle) × innerR
     y1 = cy − cos(angle) × innerR
     x2 = cx + sin(angle) × outerR
     y2 = cy − cos(angle) × outerR
     colour: white, round line caps
```

**Knob component bounds** (set in `resized()`):
```
bounds: (colX - kKnobW/2, kKnobCY - kKnobW/2, kKnobW, kKnobW)
```

---

### LookAndFeel 3: `PillLookAndFeel` (toggle button)

Subclasses `juce::LookAndFeel_V4`. Overrides `drawToggleButton`.

```
kPillW = 38,  kPillH = 11
corner radius = kPillH × 0.42

Inactive (gate mode):
  outline rounded rect, colour 0xff888888, stroke 1px
  text colour 0xff444444

Active (ping mode):
  fill rounded rect, colour 0xff111111
  text colour white

Label: always "PING", font 7.5pt Bold, centred
```

**Pill bounds** (set in `resized()`):
```
kLPGHeaderY  = 304,  kLPGHeaderH = 14
kPillYOffset = 9 - kPillH / 2  = 3   // aligns with bottomLeft label text centre
x: kKnobColX[1] - kPillW/2  (centred on DECAY column)
y: kLPGHeaderY + kPillYOffset
```

---

### Editor members

```cpp
FMSliderLookAndFeel   sliderLAF_;
ADSRKnobLookAndFeel   knobLAF_;
PillLookAndFeel       pillLAF_;

// FM section (top)
juce::Slider ratioSlider_, indexSlider_, feedbackSlider_, subSlider_;
std::array<juce::Label*, 4> fmLabels_;   // "RATIO", "INDEX", "FEEDBACK", "SUB"

// LPG section (bottom)
juce::Slider attackSlider_, decaySlider_, colorSlider_, outputSlider_;
std::array<juce::Label*, 4> knobNames_;  // "ATTACK", "DECAY", "COLOR", "OUTPUT"

// Ping toggle
juce::ToggleButton pingButton_;

// Attachments (APVTS wires parameter ↔ control automatically)
juce::AudioProcessorValueTreeState::SliderAttachment
    ratioAttach_, indexAttach_, feedbackAttach_, subAttach_,
    attackAttach_, decayAttach_, colorAttach_, outputAttach_;
juce::AudioProcessorValueTreeState::ButtonAttachment
    pingAttach_;
```

### Layout constants (anonymous namespace, `PluginEditor.cpp`)

```cpp
// Column centres
kFMColX[]   = { 83, 209, 336, 463 }
kKnobColX[] = { 83, 210, 337, 463 }

// FM sliders
kTrackTop = 95,   kTrackBot = 255,  kTrackHeight = 160,  kTrackWidth = 5
kThumbW   = 42,   kThumbH   = 13,   kTickCount   = 10
kSliderW  = 44
kSliderTop = kTrackTop - kThumbH/2       // 89
kSliderH   = kTrackBot - kSliderTop + kThumbH/2  // 172

// FM labels
kFMLabelY = 271,  kFMLabelH = 14,  kFMLabelW = 80

// Knobs
kKnobR  = 26
kKnobCY = 372
kKnobW  = kKnobR * 2 + 24  // 76
kRotaryStart = π × 1.25
kRotaryEnd   = π × 2.75

// Knob labels
kKnobLabelY = 412,  kKnobLabelH = 14,  kKnobLabelW = 80

// LPG section header
kLPGHeaderY = 304,  kLPGHeaderH = 14

// Ping pill
kPillW = 38,  kPillH = 11
kPillYOffset = 9 - kPillH / 2  // = 3
```

### `paint(Graphics& g)`

```cpp
// 1. Silver-grey base
g.setColour(juce::Colour(0xffd8d8d8));
g.fillAll();

// 2. Subtle top-to-bottom gradient overlay
juce::ColourGradient overlay(
    juce::Colour(0x18ffffff), 0, 0,
    juce::Colour(0x18000000), 0, (float)getHeight(), false);
g.setGradientFill(overlay);
g.fillAll();

// 3. Section rounded rects, labels, title, subtitle — see PluginEditor.cpp
```

---

## LPG signal flow

```
Note On  → gate_ = true, trigger_ = RISING_EDGE, lpg_gate_target_ = velocity_
           (pingMode: lpg_envelope_.Trigger())

Per block (gate mode):
  ramp lpg_gate_level_ → lpg_gate_target_ at atk_step
  lpg_envelope_.ProcessLP(lpg_gate_level_, short_decay, decay_tail, color)

Per block (ping mode):
  lpg_envelope_.ProcessPing(pitch_proportional_attack, short_decay, decay_tail, color)

Note Off → gate_ = false, trigger_ = LOW, lpg_gate_target_ = 0

engine output → sub blend → lpg_.Process(gain, freq, hf_bleed, out_) → outputGain
```

---

## Pitch correction

Plaits' internal `a0` constant is derived from its hardware sample rate (47872.34 Hz):

```cpp
pitch_correction_ = 12.0f * std::log2f(47872.34f / (float)sampleRate);
```

Added to every `params.note` so MIDI note 69 (A4) produces 440 Hz at any host sample rate.

---

## Build, install, validate

```bash
# Configure (from 2-OP/)
/opt/homebrew/bin/cmake -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=$(xcrun -f clang) \
    -DCMAKE_CXX_COMPILER=$(xcrun -f clang++)

# Build
/opt/homebrew/bin/cmake --build build --config Release

# Install + sign
rm -rf ~/Library/Audio/Plug-Ins/Components/2-OP.component
cp -R build/TwoOpFM_artefacts/Release/AU/2-OP.component \
      ~/Library/Audio/Plug-Ins/Components/
codesign --force --sign "-" \
      ~/Library/Audio/Plug-Ins/Components/2-OP.component

# Validate
auval -v aumu TWOP CVDA
```
