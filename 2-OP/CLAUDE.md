# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Plugin identity

| Field | Value |
|---|---|
| Product name | 2-OP |
| AU type | `aumu` (instrument) |
| Manufacturer code | CVDA |
| Plugin code | TWOP |
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
auval -v aumu TWOP CVDA
```

## Architecture

### DSP engine (`PluginProcessor.h/.cpp`)

The processor wraps Plaits' `FMEngine` — a 2-operator phase-modulation synth from the Mutable Instruments Eurorack firmware. Key details:

- **Block size**: Engine processes 12 samples at a time (`plaits::kBlockSize`). `processBlock` loops over the host buffer in 12-sample chunks.
- **Pitch correction**: Plaits' `a0` constant is derived from a hardcoded 47872.34 Hz hardware rate. A per-session correction (`12 × log2(47872.34 / hostSampleRate)`) is added to every `params.note` in `prepareToPlay`.
- **Parameter mapping**: `ratio → params.harmonics`, `index → params.timbre`, `feedback → params.morph`. All four FM params are smoothed via `SmoothedValue<float>` (20 ms ramp). ADSR params are read raw from APVTS once per block.
- **Sub oscillator**: `aux_[]` from `engine_.Render()` is the sub (half-frequency carrier). The `sub` parameter blends it into the main output before writing to the host buffer.
- **Mono safety**: `right` pointer is only obtained when `numCh > 1`. Writes to it are guarded with a null check.

### ADSR envelope (`ADSREnv` in `PluginProcessor.h`)

A custom linear ADSR applied per-sample after the engine renders each chunk. `noteOn()` ramps from current level (no click on retrigger). Release captures the current level as `releaseStart` for a linear ramp to zero. The engine keeps rendering during release so the pitch is maintained.

### MIDI handling

Monophonic, last-note priority. `processBlock` iterates all MIDI events at block start. Note-off only triggers release if the released note matches `sounding_note_`. Pitch bend is ±2 semitones.

### Editor (`PluginEditor.h/.cpp`)

- 400×300 px panel, 4 columns at x = 50, 150, 250, 350.
- Top section: 4 `LinearVertical` sliders (RATIO, INDEX, FDBK, SUB) with `FMSliderLookAndFeel` — flat matte-black 28×10 thumb, white hairline, 10 flanking tick marks, 4 px dark track.
- Bottom section: 4 `RotaryVerticalDrag` knobs (ATK, DCY, SUS, REL) with `ADSRKnobLookAndFeel` — matte-black circle r=16, white indicator line r=5→12, 270° sweep.
- All layout constants are in the anonymous namespace at the top of `PluginEditor.cpp`.
- APVTS attachments are constructed in the editor's initialiser list; no manual sync needed.

## External dependencies

All from `/Users/chris/src/github/eurorack`:

| File | Purpose |
|---|---|
| `plaits/dsp/engine/fm_engine.h/.cc` | Main 2-op FM engine |
| `plaits/resources.h/.cc` | Lookup tables (LUT `lut_fm_frequency_quantizer`: 130 entries, maps 0–1 → semitone offsets −12 to +36) |
| `stmlib/dsp/units.cc` | Pitch ratio LUTs required at link time |

`TEST=1` compile definition bypasses ARM VFP inline assembly in `stmlib/dsp/dsp.h`, required for x86_64 cross-compilation.
