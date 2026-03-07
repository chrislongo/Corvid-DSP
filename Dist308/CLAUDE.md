# CLAUDE.md

This file provides guidance to Claude Code when working in this directory.

## Build

**Configure** (once, or after CMakeLists.txt changes):
```bash
/opt/homebrew/bin/cmake -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=$(xcrun -f clang) \
    -DCMAKE_CXX_COMPILER=$(xcrun -f clang++)
```

**Build:**
```bash
/opt/homebrew/bin/cmake --build build --config Release
```

**Install + sign:**
```bash
cp -R "build/Dist308_artefacts/Release/AU/Dist308.component" \
      "$HOME/Library/Audio/Plug-Ins/Components/"
codesign --force --sign "-" "$HOME/Library/Audio/Plug-Ins/Components/Dist308.component"
```

**Validate AU:**
```bash
auval -v aufx D308 CVDA
```

There are no automated tests. `auval` is the primary correctness check.

## Architecture

Standard JUCE 4-file plugin split:

- `src/PluginProcessor.h/.cpp` — `Dist308AudioProcessor`: all DSP, APVTS, state serialisation.
- `src/PluginEditor.h/.cpp` — `Dist308AudioProcessorEditor` (320×200 px) + `BlackKnobLookAndFeel`.

**Signal chain:**
```
Input → [HPF ~180 Hz] → [Pre-clip LPF (LM308 GBW model)] → [Gain + tanh saturation (DISTORTION)] → [One-pole LPF (FILTER)] → [Output gain (VOLUME)] → Output
```

**Parameters:**
- `distortion` [0–100, skew centre 20, default 35] → gain `pow(1047, d/100)`; pre-clip LPF fc = `5e6 / gain`
- `filter`     [0–100, linear, default 50]         → cutoff `475 * pow(22000/475, f/100)` (475 Hz at 0%, 22 kHz at 100%)
- `volume`     [0–100, skew centre 35, default 75]  → output gain `(v/100)²`

**SmoothedValue** — one per channel per parameter (3×2 = 6 instances), targets set per block, advanced per sample.

## Plugin Identity

| Field | Value |
|---|---|
| Product name | Dist308 |
| CMake target | Dist308 |
| Manufacturer code | CVDA |
| Plugin code | D308 |
| AU type | aufx (kAudioUnitType_Effect) |
| AU export prefix | Dist308AU |
| Bundle ID | com.CorvidAudio.Dist308 |
| JUCE path | `/Users/chris/src/github/JUCE` |
