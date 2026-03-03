# 2-OP — Implementation Design

This document translates REQUIREMENTS.md into a concrete implementation plan. Read alongside the root CLAUDE.md for toolchain and JUCE conventions.

---

## Source layout

```
2-OP/
├── CMakeLists.txt
├── CLAUDE.md           (to be created after first build)
├── REQUIREMENTS.md
├── DESIGN.md           (this file)
├── design/
│   └── 2-op-mockup.svg
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
)

target_include_directories(TwoOpFM PRIVATE ${EURORACK})

target_compile_definitions(TwoOpFM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TwoOpFM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_plugin_client
)
```

Check at integration time whether any `stmlib` `.cc` files need to be added (they typically do not — stmlib is header-only for the files 2-OP uses).

---

## PluginProcessor

### Class: `TwoOpFMAudioProcessor : public juce::AudioProcessor`

#### Headers to include

```cpp
#include <JuceHeader.h>
#include "plaits/dsp/engine/fm_engine.h"
#include "stmlib/utils/buffer_allocator.h"
```

#### Members

```cpp
// APVTS — public so editor can attach sliders
juce::AudioProcessorValueTreeState apvts;

// Smoothed parameters (one set; mono engine)
juce::SmoothedValue<float> ratioSmoothed, indexSmoothed, feedbackSmoothed, subSmoothed;
// 20 ms ramp time, set in prepareToPlay

// Plaits engine
plaits::FMEngine engine;
char engine_buffer[plaits::FMEngine::kMaxBlockSize * sizeof(float) * 16]; // allocator scratch
stmlib::BufferAllocator allocator;

// Voice state
int  soundingNote = -1;        // MIDI note currently held (-1 = silent)
bool triggerRising = false;    // true for the very first block after note-on
plaits::EngineParameters params;

// Output buffers (kBlockSize = 12 samples)
float out[plaits::FMEngine::kMaxBlockSize];
float aux[plaits::FMEngine::kMaxBlockSize];
bool  already_enveloped;       // output flag from Render (unused but required)
```

#### APVTS layout helper (static factory)

```cpp
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
```

Returns four `AudioParameterFloat` entries:

| ID         | Name       | Range | Default |
|------------|------------|-------|---------|
| `ratio`    | "Ratio"    | 0–1   | 0.5     |
| `index`    | "Index"    | 0–1   | 0.3     |
| `feedback` | "Feedback" | 0–1   | 0.5     |
| `sub`      | "Sub"      | 0–1   | 0.0     |

All linear, no skew.

#### `prepareToPlay(sampleRate, samplesPerBlock)`

1. Initialise the `BufferAllocator` over `engine_buffer`.
2. Call `engine.Init(&allocator)`.
3. Set `a0` override: the engine exposes `a0` as a public member (or patch it before each `Render` call via `params` — see below).
4. Configure all four `SmoothedValue` instances: `reset(sampleRate)`, `setTargetValue(default)`, `setRampDurationSeconds(0.02f)`.

> `a0` patching: `plaits::FMEngine` stores `a0_` internally after `Init`. To override, compute `a0 = (440.0 / 8.0) / sampleRate` and write it before each `Render` call using the approach in the Plaits source (check if `a0_` is exposed; if not, subclass or patch post-`Init` via a pointer cast — keep it simple).

#### `processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi)`

```
1. Process all pending MIDI messages (see MIDI section).
2. Zero the output buffer.
3. Split host buffer into 12-sample chunks:
   for (int offset = 0; offset < numSamples; offset += kBlockSize):
     a. Advance smoothed values by kBlockSize steps.
     b. Fill params:
          params.note      = currentNote + pitchBendSemitones
          params.harmonics = ratioSmoothed.getCurrentValue()
          params.timbre    = indexSmoothed.getCurrentValue()
          params.morph     = feedbackSmoothed.getCurrentValue()
          params.accent    = velocity01
          params.trigger   = currentTriggerState
     c. engine.Render(params, out, aux, kBlockSize, &already_enveloped)
     d. After first block since note-on, set triggerState = HIGH
     e. Mix: for each sample i:
            float mixed = out[i] + subSmoothed.getNextValue() * aux[i]
            buffer.setSample(0, offset+i, mixed)
            buffer.setSample(1, offset+i, mixed)
4. If trigger was TRIGGER_LOW this block and note is off, reset voice state.
```

#### MIDI handling

Process `MidiBuffer` at the start of `processBlock`. For each event:

- **Note On**: set `soundingNote`, `velocity01 = velocity / 127.0f`, `currentNote = noteNumber`, trigger = `TRIGGER_RISING_EDGE` (used for the first Render call only, then switches to `TRIGGER_HIGH`)
- **Note Off**: if `noteNumber == soundingNote`, set trigger = `TRIGGER_LOW`, clear `soundingNote = -1`
- **Pitch Bend**: `pitchBendSemitones = (value - 8192) / 8192.0f * 2.0f` (±2 semitones)

Use last-note priority: a new Note On while a note is sounding immediately overrides the old note without a retriggering gap (set trigger to RISING_EDGE again).

#### State serialisation

```cpp
void getStateInformation(juce::MemoryBlock& dest) override {
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary(*xml, dest);
}

void setStateInformation(const void* data, int size) override {
    auto xml = getXmlFromBinary(data, size);
    if (xml) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

---

## PluginEditor

### Class: `TwoOpFMAudioProcessorEditor : public juce::AudioProcessorEditor`

Size: **300 × 280 px** (set in constructor via `setSize(300, 280)`).

### LookAndFeel: `FMSliderLookAndFeel : public juce::LookAndFeel_V4`

Override `drawLinearSlider`. All drawing is flat rectangles — no gradients, no shadows.

**Track** (drawn for `LinearVertical` style):
```
- Width: 4 px, centred on slider x
- Color: 0xff2a2a2a
- Rectangle with square ends (no rounded corners)
- Full travel height of the slider
```

**Thumb**:
```
- Size: 28 × 10 px, centred on current thumb position
- Fill: 0xff111111
- White hairline: 1 px horizontal line at vertical centre of thumb, full thumb width
```

**Tick marks** (drawn in `drawLinearSlider` before track and thumb):
```
- 10 evenly spaced ticks between endpoints (exclude endpoints)
  spacing = trackHeight / 11.0f; positions at i * spacing, i = 1..10
- Each tick: two short lines flanking the track
  left:  x1 = centre - 9,  x2 = centre - 3
  right: x1 = centre + 3,  x2 = centre + 9
- Color: 0xff888888, stroke width 1 px
```

Override `getSliderThumbRadius` to return 0 (thumb is a rectangle, not a circle).

### Editor members

```cpp
FMSliderLookAndFeel laf;

juce::Slider ratioSlider, indexSlider, feedbackSlider, subSlider;

juce::AudioProcessorValueTreeState::SliderAttachment
    ratioAttach, indexAttach, feedbackAttach, subAttach;

juce::Label ratioLabel, indexLabel, feedbackLabel, subLabel;
```

Attachments are constructed with `(apvts, "ratio", ratioSlider)` etc. — this wires parameter sync automatically.

### Layout constants

```
Column centres (x): 55, 118, 182, 245
Track top y:   52
Track bottom y: 212   (height = 160 px)
Label y:       234    (centred text baseline)
Value y:       252
```

Each slider bounds: `setBounds(centre - 14, 52, 28, 160)` — thumb extends 14 px either side of the 4 px track, which is fine since `drawLinearSlider` handles the full draw.

Actually, give each slider a wider bounds so the click target is comfortable:

```
Slider bounds: x = centre - 20, y = 40, width = 40, height = 192
```

The LookAndFeel draws the track centred within whatever bounds are given.

### Label setup (in constructor)

```cpp
for each label:
    label.setText("RATIO" / "INDEX" / "FDBK" / "SUB", juce::dontSendNotification)
    label.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)).boldened())
    label.setColour(juce::Label::textColourId, juce::Colour(0xff444444))
    label.setJustificationType(juce::Justification::centred)
    label.setBounds(centre - 24, 226, 48, 14)
```

### Value textbox

Use the built-in JUCE slider textbox below the label:

```cpp
slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 16)
slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff333333))
slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack)
slider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack)
```

Position the textbox region at y=244 by sizing the slider bounds to include it, or by using a separate `Label` that observes the parameter — the built-in textbox is simpler.

### `paint(Graphics& g)`

```cpp
// 1. Silver-grey base
g.fillAll(juce::Colour(0xffd8d8d8));

// 2. Top-to-bottom gradient overlay
juce::ColourGradient overlay(
    juce::Colour(0x18ffffff), 0, 0,
    juce::Colour(0x18000000), 0, (float)getHeight(),
    false);
g.setGradientFill(overlay);
g.fillRect(getLocalBounds());

// 3. Plugin title
g.setColour(juce::Colour(0xff222222));
g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)).boldened());
g.drawText("2-OP", getLocalBounds().withHeight(44), juce::Justification::centred);
```

No other painting needed — sliders and labels are components that paint themselves.

---

## Trigger state machine

```
soundingNote == -1 (idle):
    Note On  → soundingNote=N, trigger=RISING_EDGE, firstBlock=true

firstBlock == true:
    Render(RISING_EDGE)
    → firstBlock=false, trigger=HIGH

trigger == HIGH:
    Render(HIGH) each block while note held

Note Off matches soundingNote:
    trigger=TRIGGER_LOW
    Render(TRIGGER_LOW) — one block
    → soundingNote=-1, trigger stays LOW / NONE
```

`plaits::TriggerState` values: `TRIGGER_LOW = 0`, `TRIGGER_RISING_EDGE`, `TRIGGER_HIGH`.

---

## Pixel-exact geometry (from SVG)

| Element | Value |
|---------|-------|
| Panel size | 300 × 280 px |
| Column centres | 55, 118, 182, 245 |
| Track rect | x=centre−2, y=52, w=4, h=160 |
| Thumb rect | x=centre−14, y=thumbCentre−5, w=28, h=10 |
| Hairline | x1=centre−14, x2=centre+14, y=thumbCentre |
| Tick flanks | left: centre±(3..9), right: centre±(3..9), y= 52 + i*160/11, i=1..10 |
| Label baseline | y=234, centred on column |
| Value baseline | y=252, centred on column |
| Title baseline | y=32, centred at x=150 |

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
