# Dist308 — Requirements

Distortion plugin modelled on the ProCo Rat pedal. Reference schematic: https://www.electrosmash.com/proco-rat

---

## 1. Plugin identity

| Field | Value |
|---|---|
| Product name | Dist308 |
| CMake target | `Dist308` |
| Company | CorvidAudio |
| Manufacturer code | `CVDA` |
| Plugin code | `D308` |
| AU type | `aufx` (kAudioUnitType_Effect) |
| AU export prefix | `Dist308AU` |
| Bundle ID | `com.CorvidAudio.Dist308` |
| Formats | AU, Standalone |
| I/O | Stereo in → Stereo out (mono↔mono also supported) |

The name "Dist308" references the LM308 op-amp in the original Rat's gain stage.

---

## 2. Parameters

Three knobs matching the physical pedal.

### DISTORTION
- ID: `"distortion"`, range 0–100, skew centre 20, default 35, unit `" %"`
- Smoothing: 50 ms
- Maps to input gain: `pow(10.0, d * 3.35 / 100.0)` → 1× at 0, ~2240× (+67 dB) at 100

### FILTER
- ID: `"filter"`, range 0–100, linear, default 0, unit `" %"`
- Smoothing: 20 ms
- Inverse log cutoff: 0 → 22 kHz (open), 100 → 475 Hz (heavy cut) — matches the real Rat's backwards filter taper
- Mapping: `fc = 22000 * pow(475.0/22000.0, f/100.0)` (exponential interpolation)

### VOLUME
- ID: `"volume"`, range 0–100, skew centre 35, default 75, unit `" %"`
- Smoothing: 20 ms
- Output gain: `pow(normVolume, 2.0)` (squared law, `normVolume = v / 100.0`)

All parameters live in APVTS; editor connects via `SliderAttachment` — no manual sync.

---

## 3. DSP signal chain

```
Input → [Input gain] → [Hard clipper w/ soft knee] → [One-pole LPF] → [Output gain] → Output
```

### Input gain
Single multiply by mapped `inputGain` scalar per sample (derived from smoothed DISTORTION).

### Clipper
Piecewise cubic soft knee at threshold T = 0.9, symmetric (models anti-parallel 1N914 silicon diodes):

```
|x| ≤ T          →  x  (linear pass-through)
T < |x| ≤ 1      →  sign(x) * cubic_blend(|x|, T, 1.0)
|x| > 1          →  sign(x) * 1.0  (hard clip)
```

The cubic blend maps the knee region smoothly to ±1 with zero first derivative at |x| = 1.

### Filter
One-pole IIR low-pass; coefficient updated per-block from smoothed cutoff frequency:

```cpp
float rc    = 1.0f / (juce::MathConstants<float>::twoPi * fc);
float dt    = 1.0f / sampleRate;
float coeff = dt / (rc + dt);
// per sample:
y[n] = coeff * x[n] + (1.0f - coeff) * y[n-1];
```

- Per-channel `filterState[2]`, zeroed in `prepareToPlay`

### Output gain
Per-sample multiply from smoothed VOLUME scalar.

### Implementation notes
- Wrap `processBlock` body in `juce::ScopedNoDenormals`
- Per-channel `juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed[2]` and `filterCutoffSmoothed[2]` — same pattern as Warm's `driveSmoothed[2]`

---

## 4. UI layout

- **Size**: 320 × 200 px
- **Background**: `#d8d8d8` + subtle top→bottom gradient overlay (`0x18ffffff` → `0x18000000`) — identical to Warm
- **Knob diameter**: 72 px (three across)
- **Look-and-feel**: replicate Warm's `MetallicKnobLookAndFeel`; rename to `BlackKnobLookAndFeel` in Dist308
- **Column centres** (x): 53, 160, 267 px
- **Knob vertical centre** (y): ~105 px
- **Labels**: 10pt bold, `#111111`, all-caps, centred above each knob, positioned below the knobs
- **Text box**: `TextBoxBelow`, 60 × 18 px, transparent background

Knob order left→right: **DISTORTION — FILTER — VOLUME**

Do not include knob value % in the UI.

---

## 5. File structure

```
Dist308/
├── CMakeLists.txt
├── CLAUDE.md
├── docs/
│   ├── IDEA.md
│   └── REQUIREMENTS.md
└── src/
    ├── PluginProcessor.h
    ├── PluginProcessor.cpp   (class: Dist308AudioProcessor)
    ├── PluginEditor.h
    └── PluginEditor.cpp      (class: Dist308AudioProcessorEditor)
```

---

## 6. Build configuration

Same structure as `Warm/CMakeLists.txt` with the following overrides:

- `PLUGIN_CODE D308`
- `PRODUCT_NAME "Dist308"`
- `BUNDLE_ID com.CorvidAudio.Dist308`
- `AU_EXPORT_PREFIX Dist308AU`
- No eurorack dependency, no MIDI flags
- Same `target_link_libraries` set as Warm (no `juce_audio_devices` / `juce_audio_utils` beyond what Warm already links)

---

## 7. Validation

```bash
# Copy and sign after building
cp -r Dist308/build/Dist308_artefacts/Release/AU/Dist308.component \
      ~/Library/Audio/Plug-Ins/Components/
codesign --force --deep --sign - \
      ~/Library/Audio/Plug-Ins/Components/Dist308.component

auval -v aufx D308 CVDA
```

Manual listening checks:
- Sine at DISTORTION 0 % → clean output, no audible harmonics
- Sine at DISTORTION 50 % → moderate clipping, clear harmonic content
- Sine at DISTORTION 100 % → near-square wave
- FILTER 0 % → full bandwidth; FILTER 100 % → heavy LPF attenuation
- Slow sweeps of all three knobs → no zipper noise
- Silence in → silence out (denormals suppressed)

---

## 8. Future enhancements (out of scope for v1.0)

- HF pre-emphasis shelf (+3 dB above ~1.5 kHz before clipper) — models LM308's 100 pF feedback cap frequency response
- Asymmetric diode thresholds (introduces even harmonics, closer to real Rat asymmetry)
- 10 Hz input high-pass to block DC offset
- 2× oversampling to reduce aliasing at extreme gain settings
