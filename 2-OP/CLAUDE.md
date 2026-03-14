# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Plugin identity

| Field | Value |
|---|---|
| Product name | 2-OP |
| AU type | `aumu` (instrument) |
| Manufacturer code | Cvda |
| Plugin code | Twop |
| Bundle ID | `com.CorvidAudio.TwoOpFM` |
| CMake target | `TwoOpFM` |

## Build commands

Run from inside `2-OP/`:

```bash
# Configure (once, or after CMakeLists.txt changes)
/opt/homebrew/bin/cmake -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=$(xcrun -f clang) \
    -DCMAKE_CXX_COMPILER=$(xcrun -f clang++)

# Build
/opt/homebrew/bin/cmake --build build --config Release

# Install, sign, validate
rm -rf /Users/chris/Library/Audio/Plug-Ins/Components/2-OP.component
cp -R build/TwoOpFM_artefacts/Release/AU/2-OP.component /Users/chris/Library/Audio/Plug-Ins/Components/
codesign --force --sign "-" /Users/chris/Library/Audio/Plug-Ins/Components/2-OP.component
auval -v aumu Twop Cvda
```

## Architecture

### DSP engine (`PluginProcessor.h/.cpp`)

The processor wraps Plaits' `FMEngine` — a 2-operator phase-modulation synth from the Mutable Instruments Eurorack firmware. Key details:

- **Block size**: Engine processes 12 samples at a time (`plaits::kBlockSize`). `processBlock` loops over the host buffer in 12-sample chunks. Partial final chunks (when the host buffer isn't a multiple of 12) are passed directly — `FMEngine::Render` handles arbitrary sizes.
- **Pitch correction**: Plaits' `a0` constant is derived from a hardcoded 47872.34 Hz hardware rate. A per-session correction (`12 × log2(47872.34 / hostSampleRate)`) is added to every `params.note` in `prepareToPlay`.
- **Parameter mapping**: `ratio → params.harmonics`, `index → params.timbre`, `feedback → params.morph`. All four FM params are smoothed via `SmoothedValue<float>` (20 ms ramp). LPG params are read raw from APVTS once per block.
- **Velocity**: passed to `params.accent` for Plaits' internal timbre modulation, and embedded in the LPG gate level (vactrol opens to `velocity_` rather than 1.0).
- **Sub oscillator**: `aux_[]` from `engine_.Render()` is the sub (half-frequency carrier). The `sub` parameter blends it into `out_[]` in-place before the LPG is applied, so the LPG gates both signals together.
- **Trigger state**: `trigger_` tracks `TRIGGER_RISING_EDGE` (first render after note-on) → `TRIGGER_HIGH` (gate held) → `TRIGGER_LOW` (note-off). Note: `FMEngine` does not currently use the trigger field, but it is set correctly for future engine swaps.
- **Mono safety**: `right` pointer is only obtained when `numCh > 1`. Writes to it are guarded with a null check.

### LPG (`plaits::LPGEnvelope` + `plaits::LowPassGate`)

Replaces the former ADSR. The signal path is: FM engine → sub blend → `LowPassGate::Process` → output gain. Two classes from the Plaits firmware work together:

- **`LPGEnvelope`** (`plaits/dsp/envelope.h`): vactrol simulation. Called once per 12-sample block. Produces `gain_`, `frequency_`, and `hf_bleed_` used by the LowPassGate. Attack coefficient is fixed at 0.6/block (very fast); decay is nonlinear and controlled by the `decay` parameter.
- **`LowPassGate`** (`plaits/dsp/fx/low_pass_gate.h`): combined SVF lowpass filter + VCA. `hf_bleed = 0` → VCFA (filter tracks gain); `hf_bleed = 1` → pure VCA (filter bypassed).

**Decay formula** (identical to `plaits/dsp/voice.cc`):
```
short_decay = (200 × kBlockSize / sr) × 2^(−96 × decay / 12)
decay_tail  = (20  × kBlockSize / sr) × 2^((−72 × decay + 12 × color) / 12) − short_decay
```

**Gate mode** (`ProcessLP`): vactrol follows a ramped gate signal. Attack time (ATK knob) controls a block-rate ramp from 0 → `velocity_`. On note-off, gate drops to 0 immediately; vactrol decays per `short_decay + decay_tail`. Rendering continues until `lpg_envelope_.gain() < 0.001`.

**Ping mode** (`ProcessPing`): each note-on fires `Trigger()`. Vactrol ramps up at a pitch-proportional rate (`freq × kBlockSize × 2`), then decays freely regardless of gate state. Holding a key has no effect on amplitude.

The GATE/PING toggle is an `AudioParameterBool` (`"ping"`) exposed as a pill button in the UI.

### MIDI handling

Monophonic, last-note priority. MIDI events are processed **at their exact `samplePosition`** within the host buffer — the render loop splits the buffer at each event boundary so timing is sample-accurate. Note-off only triggers release if the released note matches `sounding_note_`. Pitch bend is ±2 semitones.

### Editor (`PluginEditor.h/.cpp`)

- 545×455 px panel.
- Top section ("FM CONTROLS"): 4 `LinearVertical` sliders (RATIO, INDEX, FEEDBACK, SUB) with `FMSliderLookAndFeel` — flat matte-black 42×13 thumb, white hairline, 10 flanking tick marks, 5 px dark track. Columns at x = 83, 209, 336, 463.
- Bottom section ("LPG + OUTPUT"): 4 `RotaryVerticalDrag` knobs (ATTACK, DECAY, COLOR, OUTPUT) with `ADSRKnobLookAndFeel` — matte-black circle r=26, white indicator line, 270° sweep. Columns at x = 83, 210, 337, 463.
- GATE/PING pill toggle: `PillLookAndFeel` on a `juce::ToggleButton`, 56×11 px, centered on the OUTPUT column (x=463), vertically centered in the section header row. Outlined = GATE, filled matte-black = PING.
- All layout constants are in the anonymous namespace at the top of `PluginEditor.cpp`.
- APVTS attachments are constructed in the editor's initialiser list; no manual sync needed.

## External dependencies

All from `/Users/chris/src/github/eurorack`:

| File | Purpose |
|---|---|
| `plaits/dsp/engine/fm_engine.h/.cc` | Main 2-op FM engine |
| `plaits/dsp/envelope.h` | `LPGEnvelope` — vactrol simulation (header-only) |
| `plaits/dsp/fx/low_pass_gate.h` | `LowPassGate` — combined SVF filter + VCA (header-only) |
| `plaits/resources.h/.cc` | Lookup tables (LUT `lut_fm_frequency_quantizer`: 130 entries, maps 0–1 → semitone offsets −12 to +36) |
| `stmlib/dsp/units.cc` | Pitch ratio LUTs required at link time |
| `stmlib/dsp/filter.h` | `stmlib::Svf` used by `LowPassGate` (header-only) |

`TEST=1` compile definition bypasses ARM VFP inline assembly in `stmlib/dsp/dsp.h`, required for x86_64 cross-compilation.
