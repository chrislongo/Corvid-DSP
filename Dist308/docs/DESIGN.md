# Dist308 — Design

High-level design for implementation. Read alongside `REQUIREMENTS.md`.

---

## 1. Class overview

```
Dist308AudioProcessor        (PluginProcessor.h/.cpp)
  └─ owns: APVTS
  └─ owns: SmoothedValue<float>[2]  ×3  (inputGain, filterCutoff, outputGain)
  └─ owns: float filterState[2]         (per-channel IIR state)
  └─ owns: float twoPiOverSr            (precomputed for filter coefficient)

Dist308AudioProcessorEditor  (PluginEditor.h/.cpp)
  └─ owns: BlackKnobLookAndFeel
  └─ owns: Slider ×3  + SliderAttachment ×3
  └─ owns: Label  ×3
```

---

## 2. Processor

### 2.1 Header members

```cpp
// APVTS — public so editor can attach
juce::AudioProcessorValueTreeState apvts;

// Per-channel smoothed gains (same target value, advanced independently per ch)
juce::SmoothedValue<float> inputGainSmoothed[2];   // 50 ms
juce::SmoothedValue<float> filterCutoffSmoothed[2]; // 20 ms
juce::SmoothedValue<float> outputGainSmoothed[2];   // 20 ms

// One-pole IIR state, one slot per channel
float filterState[2] { 0.0f, 0.0f };

// Precomputed in prepareToPlay
float twoPiOverSr = 0.0f;
```

### 2.2 `createParameterLayout()`

```cpp
// DISTORTION — skew centre at 20
juce::NormalisableRange<float> distRange(0.f, 100.f, 0.01f);
distRange.setSkewForCentre(20.f);
layout.add(AudioParameterFloat("distortion", "Distortion", distRange, 35.f, " %"));

// FILTER — linear
layout.add(AudioParameterFloat("filter", "Filter",
    {0.f, 100.f, 0.01f}, 0.f, " %"));

// VOLUME — skew centre at 35
juce::NormalisableRange<float> volRange(0.f, 100.f, 0.01f);
volRange.setSkewForCentre(35.f);
layout.add(AudioParameterFloat("volume", "Volume", volRange, 75.f, " %"));
```

### 2.3 `prepareToPlay()`

```cpp
twoPiOverSr = juce::MathConstants<float>::twoPi / (float)sampleRate;

for (int ch = 0; ch < 2; ++ch) {
    inputGainSmoothed[ch].reset(sampleRate, 0.05);
    filterCutoffSmoothed[ch].reset(sampleRate, 0.02);
    outputGainSmoothed[ch].reset(sampleRate, 0.02);

    // Set initial values from current parameter state
    inputGainSmoothed[ch].setCurrentAndTargetValue(/*distToGain(35)*/);
    filterCutoffSmoothed[ch].setCurrentAndTargetValue(22000.f);
    outputGainSmoothed[ch].setCurrentAndTargetValue(/*volToGain(75)*/);

    filterState[ch] = 0.0f;
}
```

### 2.4 `processBlock()`

```cpp
juce::ScopedNoDenormals noDenormals;

// Compute targets from current parameter values (per-block, not per-sample)
const float d = apvts.getRawParameterValue("distortion")->load();
const float f = apvts.getRawParameterValue("filter")->load();
const float v = apvts.getRawParameterValue("volume")->load();

const float targetInputGain    = std::pow(10.f, d * 3.35f / 100.f);
const float targetCutoff       = 22000.f * std::pow(475.f / 22000.f, f / 100.f);
const float normVol            = v / 100.f;
const float targetOutputGain   = normVol * normVol;

for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
    const auto chi = static_cast<size_t>(ch);

    inputGainSmoothed[chi].setTargetValue(targetInputGain);
    filterCutoffSmoothed[chi].setTargetValue(targetCutoff);
    outputGainSmoothed[chi].setTargetValue(targetOutputGain);

    float* data = buffer.getWritePointer(ch);

    for (int i = 0; i < numSamples; ++i) {
        // 1. Input gain
        float x = data[i] * inputGainSmoothed[chi].getNextValue();

        // 2. Soft-knee clipper  (T = 0.9)
        x = clip(x);

        // 3. One-pole LPF
        const float fc    = filterCutoffSmoothed[chi].getNextValue();
        const float w     = twoPiOverSr * fc;          // 2π·fc/sr
        const float coeff = w / (w + 1.f);
        filterState[ch]   = coeff * x + (1.f - coeff) * filterState[ch];
        x = filterState[ch];

        // 4. Output gain
        data[i] = x * outputGainSmoothed[chi].getNextValue();
    }
}
```

**Clipper helper** (file-local inline or private static):

```cpp
static inline float clip(float x) noexcept {
    constexpr float T = 0.9f;
    const float ax = std::abs(x);
    if (ax <= T)  return x;
    if (ax >= 1.f) return std::copysign(1.f, x);
    // Cubic blend: maps [T,1] → [T,1] with f'(1)=0
    // t ∈ [0,1] within the knee
    const float t = (ax - T) / (1.f - T);
    const float shaped = T + (1.f - T) * (3.f*t*t - 2.f*t*t*t);
    return std::copysign(shaped, x);
}
```

The cubic `3t²−2t³` hermite step gives zero derivative at t=1 (|x|=1), producing a smooth transition into the hard clip ceiling. It is evaluated only in the narrow knee region.

### 2.5 State serialization

Identical to Warm — copy/restore APVTS XML:

```cpp
void getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}
void setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

---

## 3. Editor

### 3.1 `BlackKnobLookAndFeel`

Copy `MetallicKnobLookAndFeel` from `Warm/src/PluginEditor.cpp` verbatim, renaming the class. No functional changes needed — the knob body, drop-shadow, and white indicator line are already correct for this design language.

In the constructor, omit setting text box colours — no text box is rendered (see §3.2).

### 3.2 Constructor

```cpp
Dist308AudioProcessorEditor(Dist308AudioProcessor& p)
    : AudioProcessorEditor(&p),
      distAttachment(p.apvts, "distortion", distKnob),
      filterAttachment(p.apvts, "filter",   filterKnob),
      volumeAttachment(p.apvts, "volume",   volumeKnob)
{
    setSize(320, 200);

    for (auto* knob : { &distKnob, &filterKnob, &volumeKnob }) {
        knob->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);  // no value display
        knob->setLookAndFeel(&blackKnobLAF);
        addAndMakeVisible(knob);
    }

    for (auto [label, text] : { std::pair{&distLabel,   "DISTORTION"},
                                 std::pair{&filterLabel, "FILTER"},
                                 std::pair{&volumeLabel, "VOLUME"} }) {
        label->setText(text, juce::dontSendNotification);
        label->setFont(juce::Font(juce::FontOptions().withHeight(10.f).withStyle("Bold")));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff111111));
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }
}
```

### 3.3 `paint()`

Identical pattern to Warm:

```cpp
void paint(juce::Graphics& g) override {
    g.setColour(juce::Colour(0xffd8d8d8));
    g.fillAll();
    juce::ColourGradient overlay(juce::Colour(0x18ffffff), 0, 0,
                                  juce::Colour(0x18000000), 0, (float)getHeight(), false);
    g.setGradientFill(overlay);
    g.fillAll();
}
```

### 3.4 `resized()` geometry

```
Column centres:  cx = { 53, 160, 267 }
Knob radius:     36  (setBounds uses diameter 72)
Label height:    15,  gap = 4
Group height:    r*2 + gap + labelH  =  91 px
cy:              (getHeight() − groupH) / 2 + r  →  90 px at 200px panel height
```

```cpp
void resized() override {
    constexpr int cx[3]  = { 53, 160, 267 };
    constexpr int r      = 36;
    constexpr int gap    = 4;
    constexpr int labelH = 15;
    constexpr int labelW = 100;

    const int groupH = r * 2 + gap + labelH;
    const int cy     = (getHeight() - groupH) / 2 + r;

    juce::Slider* knobs[]  = { &distKnob, &filterKnob, &volumeKnob };
    juce::Label*  labels[] = { &distLabel, &filterLabel, &volumeLabel };

    for (int i = 0; i < 3; ++i) {
        knobs[i] ->setBounds(cx[i] - r,        cy - r,       r*2,    r*2);
        labels[i]->setBounds(cx[i] - labelW/2, cy + r + gap, labelW, labelH);
    }
}
```

---

## 4. Decisions and rationale

| Decision | Rationale |
|---|---|
| `NoTextBox` on all sliders | REQUIREMENTS §4 specifies no knob value display in UI |
| Per-channel `SmoothedValue[2]` | Follows Warm's pattern; smoothing state advances independently for each channel's sample loop |
| Filter coeff computed per-sample | Cutoff is smoothed so the coefficient changes every sample; `twoPiOverSr` is precomputed to reduce the per-sample cost to one multiply and one add |
| Clipper threshold T=0.9 | Leaves 10% knee region — wide enough for an audible soft transition, narrow enough to feel like a diode clipper rather than a soft saturator |
| No `isBusesLayoutSupported` override | Default JUCE behaviour already accepts stereo↔stereo and mono↔mono; explicit override needed only if rejecting configs, not accepting |

---

## 5. Files to create

| File | Contents |
|---|---|
| `CMakeLists.txt` | Copied from `Warm/CMakeLists.txt`; substitute D308 codes (see REQUIREMENTS §6) |
| `CLAUDE.md` | Build instructions, AU codes, auval command |
| `src/PluginProcessor.h` | Class declaration matching §2.1 |
| `src/PluginProcessor.cpp` | Implementation per §2.2–2.5 |
| `src/PluginEditor.h` | Class declaration with `BlackKnobLookAndFeel`, three `Slider`/`Label`/`SliderAttachment` members |
| `src/PluginEditor.cpp` | Implementation per §3 |
