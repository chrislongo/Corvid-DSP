# 2-OP — Implementation Design

This document translates REQUIREMENTS.md into a concrete implementation plan. Read alongside the root CLAUDE.md for toolchain and JUCE conventions.

---

## Source layout

```
2-OP/
├── CMakeLists.txt
├── CLAUDE.md
├── REQUIREMENTS.md
├── DESIGN.md           (this file)
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

### ADSREnv (in PluginProcessor.h)

A lightweight linear ADSR struct — no dependencies on JUCE or Plaits.

```cpp
struct ADSREnv {
    enum State { Idle, Attack, Decay, Sustain, Release } state = Idle;
    float level        = 0.0f;
    float releaseStart = 0.0f;  // level captured at note-off for linear release

    void noteOn()  { state = Attack; }  // ramps from current level (no re-trigger click)
    void noteOff() {
        if (state == Idle) return;
        if (level <= 0.0f) { state = Idle; return; }
        releaseStart = level;
        state = Release;
    }
    bool active() const { return state != Idle; }
    void reset()        { state = Idle; level = 0.0f; releaseStart = 0.0f; }

    float processSample(float a, float d, float s, float r, float sr) noexcept;
};
```

`processSample` advances state per-sample:
- **Attack**: ramp from current level to 1.0 over `a` seconds; transition to Decay at 1.0
- **Decay**: ramp from 1.0 to `s` over `d` seconds; transition to Sustain at `s`
- **Sustain**: hold at `s`
- **Release**: ramp from `releaseStart` to 0 over `r` seconds; transition to Idle at 0

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
int   sounding_note_        = 60;   // kept after note-off so release has a pitch
float velocity_             = 1.0f;
float pitch_bend_semitones_ = 0.0f;
float pitch_correction_     = 0.0f; // compensates Plaits' hardcoded a0
bool  gate_                 = false; // true while key is physically held

ADSREnv env_;

// FM smoothers — 20 ms ramp, one SmoothedValue per parameter
juce::SmoothedValue<float> ratio_smoothed_;
juce::SmoothedValue<float> index_smoothed_;
juce::SmoothedValue<float> feedback_smoothed_;
juce::SmoothedValue<float> sub_smoothed_;
```

#### APVTS layout

| ID         | Name       | Range (linear) | Default |
|------------|------------|----------------|---------|
| `ratio`    | "Ratio"    | 0–1            | 0.5     |
| `index`    | "Index"    | 0–1            | 0.3     |
| `feedback` | "Feedback" | 0–1            | 0.5     |
| `sub`      | "Sub"      | 0–1            | 0.0     |
| `attack`   | "Attack"   | 0.001–2.0 s    | 0.008   |
| `decay`    | "Decay"    | 0.001–4.0 s    | 0.001   |
| `sustain`  | "Sustain"  | 0–1            | 1.0     |
| `release`  | "Release"  | 0.001–4.0 s    | 0.001   |

ADSR parameters are read raw per block (no smoothing needed — the envelope itself provides smooth amplitude).

#### `prepareToPlay(sampleRate, samplesPerBlock)`

```
1. allocator_.Init(engine_buffer_, sizeof(engine_buffer_))
2. engine_.Init(&allocator_)
3. pitch_correction_ = 12.0f * log2f(47872.34f / sampleRate)
   // Plaits' a0 is hardcoded for its hardware rate (47872.34 Hz);
   // this semitone offset corrects pitch at any host sample rate.
4. env_.reset()
5. For each SmoothedValue: reset(sampleRate, 0.02), setCurrentAndTargetValue(default)
```

#### `processBlock`

```
1. For each MIDI event:
   - Note On:  sounding_note_ = note, velocity_ = vel/127, gate_ = true, env_.noteOn()
   - Note Off: if note == sounding_note_ → gate_ = false, env_.noteOff()
               DO NOT clear sounding_note_ — engine needs it for release pitch
   - Pitch wheel: pitch_bend_semitones_ = (raw - 8192) / 8192.0f * 2.0f

2. Early exit: if (!gate_ && !env_.active()) return;

3. Read ADSR params once per block (atk, dcy, sus, rel)

4. Update FM smoother targets from APVTS

5. Split buffer into kBlockSize (12-sample) chunks:
   a. Advance all four smoothers exactly chunk samples
   b. Fill params: note = sounding_note_ + pitch_bend_ + pitch_correction_
                   harmonics = ratio, timbre = index, morph = feedback
                   accent = velocity_, trigger = TRIGGER_HIGH
   c. engine_.Render(params, out_, aux_, chunk, &already_enveloped_)
   d. Per-sample: env_amp = env_.processSample(atk, dcy, sus, rel, sr)
                  s = (out_[i] + sub_val * aux_[i]) * env_amp
                  write to left and right channels
```

`getTailLengthSeconds()` returns 4.0 (maximum release time).

---

## PluginEditor

### Panel size

**400 × 300 px**

Two sections separated by a horizontal rule:
- **Top (FM)**: 4 vertical sliders, track y=44–168
- **Separator**: thin line at y=210
- **Bottom (ADSR)**: 4 rotary knobs centred at y=246

### Column centres

```
x: 50, 150, 250, 350
```

Four evenly-spaced columns — 100 px apart, 50 px margins each side.

---

### LookAndFeel 1: `FMSliderLookAndFeel` (vertical sliders)

Subclasses `juce::LookAndFeel_V4`. Override `drawLinearSlider` and `getSliderThumbRadius`.

**`getSliderThumbRadius`** → returns `5` (= kThumbH/2). This causes JUCE to constrain
`sliderPos` to `[thumbRadius, height - thumbRadius]`, keeping the thumb fully inside the
component at both extremes.

**Slider component bounds** (set in `resized()`):
```
kSliderTop = kTrackTop - kThumbH/2 = 47
kSliderH   = kTrackBot - kSliderTop + kThumbH/2 = 170
kSliderW   = 44
bounds: (colX - kSliderW/2, kSliderTop, kSliderW, kSliderH)
```

**`drawLinearSlider`** (all drawing in local slider coordinates, y=0 is kSliderTop):
```
localTrackTop = kTrackTop - kSliderTop  = 5
localTrackBot = kTrackBot - kSliderTop  = 165
trackH        = 160

Tick marks (10 ticks, colour 0xff888888):
  ty = localTrackTop + i * trackH / (kTickCount + 1),  i = 1..10
  left:  (cx-9, ty) → (cx-3, ty)
  right: (cx+3, ty) → (cx+9, ty)

Track (colour 0xff2a2a2a):
  fillRect(cx - 2, localTrackTop, 4, trackH)

Thumb (matte-black, colour 0xff111111):
  thumbCy = sliderPos   (provided by JUCE, clamped to [5, 165])
  fillRect(cx - 14, thumbCy - 5, 28, 10)

Hairline (white, 1px):
  drawLine(cx - 14, thumbCy, cx + 14, thumbCy)
```

**Colour overrides** (in constructor):
```
textBoxText:       0xff333333
textBoxBackground: transparent
textBoxOutline:    transparent
```

---

### LookAndFeel 2: `ADSRKnobLookAndFeel` (rotary knobs)

Subclasses `juce::LookAndFeel_V4`. Override `drawRotarySlider`.

**Style**: `juce::Slider::RotaryVerticalDrag`

**Geometry**:
```
Knob radius: 16 px
Indicator inner radius: 5
Indicator outer radius: 12
Start angle: -135° from 12 o'clock (= -juce::MathConstants<float>::pi * 0.75f)
End angle:   +135° from 12 o'clock (= +juce::MathConstants<float>::pi * 0.75f)
Total sweep: 270°
```

**`drawRotarySlider`**:
```
cx, cy = centre of component bounds
r = min(width, height) / 2 - 2   (or use fixed 16)

1. Fill circle, radius r, colour 0xff111111
2. Compute indicator angle:
     angle = startAngle + sliderPos * (endAngle - startAngle)
     (sliderPos is 0–1, provided by JUCE)
3. Draw indicator line:
     x1 = cx + sin(angle) * 5
     y1 = cy - cos(angle) * 5
     x2 = cx + sin(angle) * 12
     y2 = cy - cos(angle) * 12
     colour: white, stroke-width: 2.2f, round line caps
```

**Knob component bounds** (set in `resized()`):
```
kKnobR   = 16
kKnobW   = kKnobH = kKnobR * 2 + 4  (= 36 px, small extra for hit target)
kKnobCY  = 285   (vertical centre of knob in panel coords)
bounds: (colX - kKnobW/2, kKnobCY - kKnobR - 2, kKnobW, kKnobW)
```

---

### Editor members

```cpp
FMSliderLookAndFeel   sliderLAF_;
ADSRKnobLookAndFeel   knobLAF_;

// FM section (top)
juce::Slider ratioSlider_, indexSlider_, feedbackSlider_, subSlider_;
juce::Label  ratioLabel_,  indexLabel_,  feedbackLabel_,  subLabel_;
juce::Label  ratioValue_,  indexValue_,  feedbackValue_,  subValue_;

// ADSR section (bottom)
juce::Slider attackSlider_, decaySlider_, sustainSlider_, releaseSlider_;
juce::Label  attackLabel_,  decayLabel_,  sustainLabel_,  releaseLabel_;
juce::Label  attackValue_,  decayValue_,  sustainValue_,  releaseValue_;

// Attachments (APVTS wires parameter ↔ slider automatically)
juce::AudioProcessorValueTreeState::SliderAttachment
    ratioAttach_, indexAttach_, feedbackAttach_, subAttach_,
    attackAttach_, decayAttach_, sustainAttach_, releaseAttach_;
```

### Layout constants

```cpp
// Panel
kWidth  = 400,  kHeight = 300

// FM sliders (top section)
kColX[]    = { 50, 150, 250, 350 }
kTrackTop  = 44,   kTrackBot = 168,  kTrackWidth = 4
kThumbW    = 28,   kThumbH   = 10,   kTickCount  = 10
kSliderW   = 44
kSliderTop = kTrackTop - kThumbH/2  // = 39
kSliderH   = kTrackBot - kSliderTop + kThumbH/2  // = 134

// FM labels / values
kFMLabelY  = 176,  kFMLabelH = 14
kFMValueY  = 190,  kFMValueH = 14

// Separator
kSeparatorY = 210

// ADSR knobs (bottom section)
kKnobR   = 16
kKnobCY  = 246
kKnobW   = kKnobH = kKnobR * 2 + 4  // = 36

// ADSR labels / values
kADSRLabelY = 270,  kADSRLabelH = 14
kADSRValueY = 284,  kADSRValueH = 12
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

// 3. Plugin title
g.setColour(juce::Colour(0xff222222));
g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
g.drawText("2-OP", 0, 0, getWidth(), 44, juce::Justification::centred);

// 4. FM / ADSR section separator
g.setColour(juce::Colour(0x40000000));
g.drawLine(20.0f, (float)kSeparatorY, (float)(kWidth - 20), (float)kSeparatorY, 0.75f);
```

### Value display

Use a separate `juce::Label` per control (wired via `onValueChange` lambda). Sliders use `NoTextBox`. Labels formatted to 2 decimal places.

---

## Trigger / envelope

The Plaits `FMEngine` is always passed `TRIGGER_HIGH` (the engine itself does not control amplitude). Volume shaping is handled entirely by `ADSREnv::processSample` applied to the engine output.

```
Note On  → gate_ = true,  env_.noteOn()   → Attack ramp
Note Off → gate_ = false, env_.noteOff()  → Release ramp
No note  → gate_ = false, env_.active() = false → processBlock returns early
```

---

## Pitch correction

Plaits' internal `a0` constant is derived from its hardware sample rate (47872.34 Hz):

```cpp
pitch_correction_ = 12.0f * std::log2f(47872.34f / (float)sampleRate);
```

This is added to every `params.note` so that MIDI note 69 (A4) produces 440 Hz at any host sample rate.

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
cp -R "build/TwoOpFM_artefacts/Release/AU/2-OP.component" \
      "$HOME/Library/Audio/Plug-Ins/Components/"
codesign --force --sign "-" \
      "$HOME/Library/Audio/Plug-Ins/Components/2-OP.component"

# Validate
auval -v aumu TWOP CVDA
```
