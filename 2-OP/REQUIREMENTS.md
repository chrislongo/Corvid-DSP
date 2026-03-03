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
The Plaits engine uses a hardcoded `a0` constant derived from 47872.34 Hz. At runtime `a0` is replaced with `(440.0 / 8.0) / hostSampleRate` so pitch is accurate at any host sample rate.

### Parameter Mapping (EngineParameters)
| Slider | APVTS ID | EngineParameters field | Notes |
|--------|----------|------------------------|-------|
| Ratio | `ratio` | `harmonics` | 0–1; quantized via `lut_fm_frequency_quantizer` (128 entries) to semitone offsets −12 to +30, snapping to musical ratios: unison, P5, octave, etc. |
| Index | `index` | `timbre` | 0–1; modulation depth, internally `2 × timbre² × hf_taming` |
| Feedback | `feedback` | `morph` | 0–1; 0–0.5 = phase feedback (carrier→modulator), 0.5–1 = modulator self-feedback |
| Sub | `sub` | — | 0–1; mix level of aux buffer into main output |

### MIDI / Voice Handling
- **Note on**: capture note number; set `trigger = RISING_EDGE` for first block, then `trigger = HIGH`
- **Note off**: if matches sounding note, set `trigger = TRIGGER_LOW`
- **Pitch**: MIDI note number passed directly as `EngineParameters::note`
- **Velocity**: maps to `accent` (0–1); unused by FMEngine in v0.1 but wired for future use
- **Pitch bend**: ±2 semitone range added to `note` (stretch goal for v0.1)

## Parameters
| Name | ID | Range | Default | Taper |
|------|----|-------|---------|-------|
| Ratio | `ratio` | 0–1 | 0.5 | Linear (quantized by engine) |
| Index | `index` | 0–1 | 0.3 | Linear |
| Feedback | `feedback` | 0–1 | 0.5 | Linear |
| Sub | `sub` | 0–1 | 0.0 | Linear |

All parameters use `SmoothedValue<float>` with a 20 ms ramp to prevent zipper noise.

## I/O
- Input: no audio input (synth)
- Output: stereo (mono engine signal copied to both channels)
- MIDI input: required
- MIDI output: none

## UI
- **Size**: 300×280 px
- **Background**: silver-grey `0xffd8d8d8` with subtle top-to-bottom gradient overlay (same as Warm: `0x18ffffff` → `0x18000000`); flat, no texture
- **Controls**: 4 vertical sliders, evenly spaced across the width
  - Style: `LinearVertical`, custom `LookAndFeel`
  - **Track**: narrow (4 px wide), matte-dark `0xff2a2a2a`, flat rectangle with square caps — no rounded ends, no shadow
  - **Thumb**: flat matte-black rectangle (~28 × 10 px), wider than the track; a single 1 px white horizontal line centered on the face as position indicator; no drop shadow, no gradient — TR-1000 / vintage drum machine aesthetic
  - Thumb color: `0xff111111`; indicator line: `0xffffffff`
  - Each slider travel height: ~160 px; total control area height ~180 px
- **Labels**: slider name centered below each control, 10pt bold, dark grey `0xff444444`, all-caps
- **Value display**: numeric textbox centered below label, 10pt, transparent background, dark text `0xff333333`
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
  - Any `stmlib` `.cc` files required (check at integration time)

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
- `auval -v aumu TWOP CADSP` — primary correctness check after install
- Play MIDI notes across the full pitch range; confirm tuning accuracy
- Sweep each slider; confirm audible, artifact-free parameter changes
- Confirm Sub slider audibly blends sub oscillator
- Confirm Ratio knob snaps through quantized intervals (audible pitch steps in modulator)
