# 2-OP — Plugin Requirements

## Plugin Identity
| Field | Value |
|---|---|
| Product name | 2-OP |
| CMake target | TwoOpFM |
| Manufacturer | CorvidAudio |
| Manufacturer code | CVDA |
| Plugin code | TWOP |
| AU type | kAudioUnitType_MusicDevice (aumu) |
| AU export prefix | TwoOpFMAU |
| Bundle ID | com.CorvidAudio.TwoOpFM |
| JUCE path | `/Users/chris/src/github/JUCE` |

## Formats
- Audio Unit (AU)
- Standalone application

## DSP Engine
- Source: Plaits `FMEngine` — `/Users/chris/src/github/eurorack/plaits/dsp/engine/fm_engine.h/.cc`
- 2-operator phase-modulation synthesis: carrier oscillator, modulator oscillator, sub oscillator
- 4× oversampled internally with FIR downsampler (`lut_4x_downsampler_fir`)
- Monophonic voice (last-note priority)
- All synthesis runs in 12-sample chunks (`kBlockSize = 12`); host buffer is split accordingly
- Output: carrier signal on main out, sub oscillator (half carrier frequency) on aux out
- Sub level parameter controls blend of aux into main before the LPG is applied

### Sample Rate Adaptation
The Plaits engine uses a hardcoded `a0` constant derived from 47872.34 Hz (its Eurorack hardware rate). A semitone correction `12 × log2(47872.34 / hostSampleRate)` is added to every `params.note` in `processBlock` so pitch is accurate at any host sample rate.

### Parameter Mapping (EngineParameters)
| Slider | APVTS ID | EngineParameters field | Notes |
|--------|----------|------------------------|-------|
| Ratio | `ratio` | `harmonics` | 0–1; quantized via `lut_fm_frequency_quantizer` (130 entries) to semitone offsets −12 to +36, snapping to musical ratios |
| Index | `index` | `timbre` | 0–1; modulation depth, internally `2 × timbre² × hf_taming` |
| Feedback | `feedback` | `morph` | 0–1; 0–0.5 = phase feedback (carrier→modulator), 0.5–1 = modulator self-feedback |
| Sub | `sub` | — | 0–1; mix level of aux buffer into main output before LPG |

### LPG (Low-Pass Gate)
Amplitude and filter shaping are handled by a Plaits-style vactrol simulation (`LPGEnvelope`) driving a combined SVF lowpass filter + VCA (`LowPassGate`). This replaces a conventional ADSR.

**Decay formula** (identical to `plaits/dsp/voice.cc`):
```
short_decay = (200 × kBlockSize / sr) × 2^(−96 × decay / 12)
decay_tail  = (20  × kBlockSize / sr) × 2^((−72 × decay + 12 × color) / 12) − short_decay
```

**Gate mode** (`ProcessLP`): the LPG follows the MIDI gate. On note-on, the vactrol gate level ramps from 0 to `velocity_` at a rate controlled by the Attack knob. On note-off, the gate drops immediately and the vactrol decays per the decay formula. Rendering continues until `lpg_envelope_.gain() < 0.001`.

**Ping mode** (`ProcessPing`): each note-on fires `Trigger()` on the vactrol. The vactrol ramps up at a pitch-proportional rate (higher notes attack faster, matching Plaits behaviour) then decays freely regardless of gate state. Holding a key has no effect on amplitude.

### MIDI / Voice Handling
- **Note on**: capture note number, set `gate = true`, set trigger to `TRIGGER_RISING_EDGE`, begin LPG attack
- **Note off**: if matches sounding note, set `gate = false`, drop LPG gate target to 0; note number retained for decay-tail pitch
- **Pitch**: MIDI note number + semitone correction passed as `EngineParameters::note`
- **Velocity**: scales LPG gate level (vactrol opens to `velocity_` rather than 1.0); also passed as `params.accent` for Plaits timbre modulation
- **Pitch bend**: ±2 semitone range added to `note`

## Parameters

### FM (engine inputs)
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Ratio | `ratio` | 0–1 | 0.5 | Linear (quantized by engine LUT) |
| Index | `index` | 0–1 | 0.3 | Linear |
| Feedback | `feedback` | 0–1 | 0.5 | Linear |
| Sub | `sub` | 0–1 | 0.0 | Linear |

FM parameters use `SmoothedValue<float>` with a 20 ms ramp to prevent zipper noise.

### LPG
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Attack | `attack` | 0.005–2.0 s | 0.010 s | Skewed (exponent 0.3) |
| Decay | `decay` | 0–1 | 0.5 | Linear (Plaits internal scale) |
| Color | `color` | 0–1 | 0.0 | Linear |
| Ping Mode | `ping` | false/true | false | — |

### Output
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Output | `output` | −40–0 dB | −12 dB | Linear (dB) |

LPG and output parameters are read raw from APVTS once per block; no smoothing needed.

## I/O
- Input: no audio input (synth)
- Output: stereo (mono engine signal copied to both channels)
- MIDI input: required
- MIDI output: none

## UI
- **Size**: 545×455 px
- **Background**: silver-grey `0xffd8d8d8` with subtle top-to-bottom gradient overlay (`0x18ffffff` → `0x18000000`); flat, no texture
- **Layout**: two rounded-rect sections separated by vertical space
  - **Top section — FM CONTROLS**: 4 vertical sliders (RATIO, INDEX, FEEDBACK, SUB)
  - **Bottom section — LPG + OUTPUT**: 4 rotary knobs (ATTACK, DECAY, COLOR, OUTPUT) + PING pill toggle

**FM sliders** (top section):
- Style: `LinearVertical`, custom `FMSliderLookAndFeel`
- **Track**: 5 px wide, matte-dark `0xff2a2a2a`, flat rectangle
- **Thumb**: 42×13 px flat matte-black rectangle (`0xff111111`); single 1 px white hairline centered vertically
- **Tick marks**: 10 evenly-spaced flanking marks either side of the track, grey `0xff888888`
- Track travel: y=95–255 (160 px)
- Column centres: x = 83, 209, 336, 463

**LPG knobs** (bottom section):
- Style: `RotaryVerticalDrag`, custom `ADSRKnobLookAndFeel`
- **Body**: matte-black circle, radius 26 px (`0xff111111`)
- **Tick marks**: short radial lines around the perimeter at standard positions
- **Indicator**: white line, round caps
- Sweep: 270° (−135° to +135° from 12 o'clock)
- Knob centre: y=372; column centres: x = 83, 210, 337, 463

**PING pill toggle**:
- `juce::ToggleButton` with `PillLookAndFeel`
- 38×11 px, centered horizontally on the DECAY column (x=210)
- Vertically aligned with the "LPG + OUTPUT" section label (y≈307)
- **Outlined** = Gate mode (inactive); **filled matte-black** = Ping mode (active)
- Label always reads "PING" in both states

- **Labels**: control name centered below each control, 10pt bold, dark grey `0xff444444`, all-caps
- **Section headers**: "FM CONTROLS" and "LPG + OUTPUT", 8pt bold, `0xff555555`
- **Plugin name**: "2-OP" top-left, bold, dark grey
- Flat overall — no bevels, no drop shadows anywhere on the panel

## Build
- Build system: CMake 3.22+ with Ninja generator
- Target platforms: macOS 11.0+
- Architectures: arm64 + x86_64 (universal binary)
- `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT FALSE`
- Additional include directories: `/Users/chris/src/github/eurorack`
- Source files compiled alongside plugin:
  - `plaits/dsp/engine/fm_engine.cc`
  - `plaits/resources.cc` (lookup tables: sine, FIR, fm_frequency_quantizer, etc.)
  - `stmlib/dsp/units.cc` (pitch ratio LUTs: `lut_pitch_ratio_high`, `lut_pitch_ratio_low`)
- Compile definition `TEST=1` required to bypass ARM VFP inline assembly in `stmlib/dsp/dsp.h` (needed for x86_64 cross-compilation)

## Dependencies (from eurorack repo)
- `plaits/dsp/engine/fm_engine.h/.cc` — main engine
- `plaits/dsp/envelope.h` — `LPGEnvelope` vactrol simulation (header-only)
- `plaits/dsp/fx/low_pass_gate.h` — `LowPassGate` combined SVF filter + VCA (header-only)
- `plaits/dsp/oscillator/sine_oscillator.h` — `SinePM` function
- `plaits/dsp/downsampler/4x_downsampler.h` — FIR downsampler
- `plaits/resources.h` + `plaits/resources.cc` — all lookup tables
- `stmlib/dsp/dsp.h` — `CONSTRAIN`, `ONE_POLE`
- `stmlib/dsp/filter.h` — `stmlib::Svf` used by `LowPassGate`
- `stmlib/dsp/units.h` — `SemitonesToRatio`, `Interpolate`
- `stmlib/dsp/parameter_interpolator.h` — per-block parameter smoothing
- `stmlib/utils/buffer_allocator.h` — engine `Init()` allocator

## State
- Preset save/load via `AudioProcessorValueTreeState` XML serialisation (standard `getStateInformation` / `setStateInformation`)

## Verification
- `auval -v aumu TWOP CVDA` — primary correctness check after install
- Play MIDI notes across the full pitch range; confirm tuning accuracy
- Sweep each FM slider; confirm audible, artifact-free parameter changes
- Confirm Sub slider audibly blends sub oscillator
- Confirm Ratio slider snaps through quantized intervals (audible pitch steps in modulator)
- **Gate mode**: hold a note; confirm Attack ramp opens the LPG, note sustains, release decays on note-off
- **Ping mode**: hold a note; confirm LPG fires a single transient and decays regardless of key held
- Confirm Decay and Color knobs shape the tail in both modes
- Confirm velocity modulates both output amplitude and FM timbre
- Rapid retrigger: no glitch, LPG re-triggers cleanly
