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
- Sub level parameter controls blend of aux into main before writing to host buffer

### Sample Rate Adaptation
The Plaits engine uses a hardcoded `a0` constant derived from 47872.34 Hz (its Eurorack hardware rate). A semitone correction `12 × log2(47872.34 / hostSampleRate)` is added to every `params.note` in `processBlock` so pitch is accurate at any host sample rate.

### Parameter Mapping (EngineParameters)
| Slider | APVTS ID | EngineParameters field | Notes |
|--------|----------|------------------------|-------|
| Ratio | `ratio` | `harmonics` | 0–1; quantized via `lut_fm_frequency_quantizer` (128 entries) to semitone offsets −12 to +30, snapping to musical ratios: unison, P5, octave, etc. |
| Index | `index` | `timbre` | 0–1; modulation depth, internally `2 × timbre² × hf_taming` |
| Feedback | `feedback` | `morph` | 0–1; 0–0.5 = phase feedback (carrier→modulator), 0.5–1 = modulator self-feedback |
| Sub | `sub` | — | 0–1; mix level of aux buffer into main output |

### Amplitude Envelope
A linear ADSR (`ADSREnv`) is applied to the engine output after `Render()`:
- **Note on**: `noteOn()` begins Attack ramp from the current level (prevents re-trigger click)
- **Note off**: `noteOff()` captures current level as `releaseStart`, begins Release ramp
- The engine continues rendering during Release (sounding note is not cleared on note-off so pitch is maintained)
- `processBlock` returns early when the gate is closed and the envelope is idle, saving CPU

### MIDI / Voice Handling
- **Note on**: capture note number, set `gate = true`, trigger envelope Attack
- **Note off**: if matches sounding note, set `gate = false`, trigger envelope Release; note number retained for release pitch
- **Pitch**: MIDI note number + semitone correction passed as `EngineParameters::note`
- **Velocity**: maps to `accent` (0–1)
- **Pitch bend**: ±2 semitone range added to `note`

## Parameters

### FM (engine inputs)
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Ratio | `ratio` | 0–1 | 0.5 | Linear (quantized by engine) |
| Index | `index` | 0–1 | 0.3 | Linear |
| Feedback | `feedback` | 0–1 | 0.5 | Linear |
| Sub | `sub` | 0–1 | 0.0 | Linear |

FM parameters use `SmoothedValue<float>` with a 20 ms ramp to prevent zipper noise.

### Envelope (ADSR)
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Attack | `attack` | 0.001–2.0 s | 0.008 s | Linear |
| Decay | `decay` | 0.001–4.0 s | 0.001 s | Linear |
| Sustain | `sustain` | 0–1 | 1.0 | Linear |
| Release | `release` | 0.001–4.0 s | 0.001 s | Linear |

ADSR parameters are read raw from APVTS once per block; no smoothing needed (the envelope provides continuous amplitude transitions).

## I/O
- Input: no audio input (synth)
- Output: stereo (mono engine signal copied to both channels)
- MIDI input: required
- MIDI output: none

## UI
- **Size**: 400×300 px
- **Background**: silver-grey `0xffd8d8d8` with subtle top-to-bottom gradient overlay (`0x18ffffff` → `0x18000000`); flat, no texture
- **Layout**: two rows of controls separated by a thin horizontal rule
  - **Top row — FM**: 4 vertical sliders (RATIO, INDEX, FDBK, SUB)
  - **Bottom row — ADSR**: 4 rotary knobs (ATK, DCY, SUS, REL)
  - Column centres: x = 50, 150, 250, 350 (100 px apart, 50 px margins)

**FM sliders** (top section):
  - Style: `LinearVertical`, custom `FMSliderLookAndFeel`
  - **Track**: 4 px wide, matte-dark `0xff2a2a2a`, flat rectangle, square caps
  - **Thumb**: 28×10 px flat matte-black rectangle (`0xff111111`); single 1 px white hairline centered vertically — TR-808 / vintage drum machine aesthetic
  - **Tick marks**: 10 evenly-spaced flanking marks either side of the track, grey `0xff888888`
  - Travel height: 124 px

**ADSR knobs** (bottom section):
  - Style: `RotaryVerticalDrag`, custom `ADSRKnobLookAndFeel`
  - **Body**: matte-black circle, radius 16 px (`0xff111111`)
  - **Indicator**: white line from r=5 to r=12, 2.2 px stroke, round caps
  - Sweep: 270° (−135° to +135° from 12 o'clock)

- **Labels**: control name centered below each control, 10pt bold, dark grey `0xff444444`, all-caps
- **Value display**: numeric label centered below parameter name, 10pt, dark text `0xff555555`, transparent background
- **Plugin name**: "2-OP" top-center, 13pt bold, dark grey `0xff222222`
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
- `plaits/dsp/oscillator/sine_oscillator.h` — `SinePM` function
- `plaits/dsp/downsampler/4x_downsampler.h` — FIR downsampler
- `plaits/resources.h` + `plaits/resources.cc` — all lookup tables
- `stmlib/dsp/dsp.h` — `CONSTRAIN`, `ONE_POLE`
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
- Confirm ADSR shapes amplitude correctly: no click on note-on, clean release tail
- Rapid retrigger: no glitch, ramps up from current level
