# Warm — Design Document

## Architecture

Standard JUCE plugin split into 4 source files:

```
src/
├── PluginProcessor.h    — AudioProcessor subclass declaration
├── PluginProcessor.cpp  — DSP implementation
├── PluginEditor.h       — AudioProcessorEditor + LookAndFeel declaration
└── PluginEditor.cpp     — UI rendering implementation
```

## DSP Design

### Why tanh?
`tanh` is an odd function: `tanh(-x) = -tanh(x)`. When applied as a waveshaper,
odd functions produce only odd-order harmonics (3rd, 5th, 7th...), giving a warm,
tube-like character distinct from even-harmonic distortion.

### Algorithm
```
normAmount = amount / 100.0
drive      = 0.5 * pow(40.0, normAmount)         // exponential: 0.5 → 20.0
y          = tanh(drive * x) / tanh(drive)       // normalized: unity gain at DC
makeupGain = 1.0 - normAmount * 0.109            // gentle -1 dB correction at max
```

Drive range chosen to mirror ProCo Rat behaviour:
- 0%  → drive 0.5  (subtle even at minimum, like the Rat at low settings)
- 50% → drive ~1.8 (moderate saturation around noon)
- 75% → drive ~5.5 (heavy distortion)
- 100%→ drive 20.0 (extreme)

Drive is normalized so peak output amplitude matches peak input at all settings.
Makeup gain provides subtle level correction since saturation raises perceived loudness.

### Smoothing
`juce::SmoothedValue<float>` on the drive parameter prevents zipper noise when
the knob is moved during playback. One smoother per channel (max 2 for stereo).

### Denormal Guard
`juce::ScopedNoDenormals` applied at the top of `processBlock` to prevent
CPU spikes from near-zero floating point values in the feedback path.

## Parameter Management

`AudioProcessorValueTreeState` (APVTS) with a single parameter:
- ID: `"amount"`, range [0, 100], default 0, suffix " %"
- Taper: `NormalisableRange::setSkewForCentre(25.0f)` — log/audio pot feel, mimicking a ProCo Rat C-curve pot (skewFactor ≈ 0.5, equivalent to an x² response)

Editor uses `SliderAttachment` for automatic two-way binding.

## UI Design

### Layout (300×220 px)
```
┌─────────────────────────────────────┐
│                                     │
│           HARMONICS                 │  ← bold label above knob
│           ┌─────────┐              │
│           │  knob   │              │  ← 100×100 matte black, centred
│           └─────────┘              │
│           [  0.0 % ]               │  ← value text box
│                                     │
└─────────────────────────────────────┘
```

No plugin name displayed. No footer bar. Knob and label are vertically and
horizontally centred in the full 300×220 window.

### Background
Light silver-grey flat fill `#d8d8d8` with a subtle top-to-bottom gradient overlay
(+10% white at top, +10% black at bottom) for mild depth.

### MetallicKnobLookAndFeel (black knob style)
Custom `LookAndFeel_V4` subclass rendering layers bottom-up:

1. **Drop shadow** — translucent radial gradient below/behind the knob body
2. **Body** — pure matte black circle `#111111`
3. **Top-edge highlight** — subtle white translucent gradient on upper hemisphere
4. **Indicator line** — solid white line, 30%→78% of body radius

No metallic ring, no value arc. Minimalist Valhalla-style aesthetic.

### Typography
- Plugin name: `"Warm"`, 36pt bold, `#111111`, top-left
- Knob label: `"HARMONICS"`, 11pt bold, `#111111`, centred above knob
- Value text box: transparent background, `#1a1a1a` text

## State Persistence

```cpp
void getStateInformation(MemoryBlock& dest) {
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(ValueTree::fromXml(*xml));
}
```
