# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

cmake is at `/opt/homebrew/bin/cmake`. Xcode is **not** installed — use the Ninja generator.

**Configure** (only needed after clean or CMakeLists changes):
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
cp -R "build/OddHarmonics_artefacts/Release/AU/Warm.component" \
      "$HOME/Library/Audio/Plug-Ins/Components/"
codesign --force --sign "-" "$HOME/Library/Audio/Plug-Ins/Components/Warm.component"
```

**Validate AU:**
```bash
auval -v aufx ODDH CHRS
```

There are no automated tests. `auval` is the primary correctness check.

**DSP benchmark** (no extra installs, arm64 only):
```bash
xcrun clang++ -O3 -std=c++17 -arch arm64 -o /tmp/dsp_bench bench/dsp_bench.cpp && /tmp/dsp_bench
```
Simulates the stereo processBlock inner loop and reports CPU% vs real-time budget. Baseline: <0.03% CPU at all drive settings.

## Architecture

Standard JUCE 4-file plugin split:

- `src/PluginProcessor.h/.cpp` — `OddHarmonicsAudioProcessor`: all DSP, parameter state, and serialisation. Owns the `apvts` (public, so the editor can attach to it).
- `src/PluginEditor.h/.cpp` — `OddHarmonicsAudioProcessorEditor` (300×220 px) + `MetallicKnobLookAndFeel` (the class name is a legacy artefact — it renders a plain matte-black knob with a white indicator line, not a metallic one). The editor holds a `SliderAttachment` that wires the knob directly to the APVTS parameter — no manual sync code needed.

**Parameter flow:**
```
NormalisableRange [0,100] with setSkewForCentre(25)
  → SliderAttachment (editor)
  → apvts.getRawParameterValue("amount") (processor)
  → normAmount = amount / 100
  → drive = 0.5 * pow(40, normAmount)   // 0.5 → 20.0
  → tanh(drive * x) / tanh(drive)       // odd-harmonic waveshaper
```

**SmoothedValue** — one per channel (`driveSmoothed[2]`), 50 ms ramp, prevents zipper noise. The target is set once per `processBlock` call; per-sample `getNextValue()` does the interpolation.

## JUCE CMake Notes

- `juce_generate_juce_header(OddHarmonics)` is required — without it `<JuceHeader.h>` is not found.
- Standalone format requires `juce::juce_audio_utils` and `juce::juce_audio_devices` in `target_link_libraries`.
- Use `juce::Font(juce::FontOptions().withHeight(n))` — the `Font(float)` constructor is deprecated in this JUCE version.
- `AudioProcessorEditor` already has a `processor` member — don't shadow it in the editor subclass.
- Index `std::array<SmoothedValue>` with `static_cast<size_t>(ch)` to avoid `-Wsign-conversion`.

## Plugin Identity

| Field | Value |
|---|---|
| Product name | Warm |
| CMake target | OddHarmonics |
| Manufacturer code | CHRS |
| Plugin code | ODDH |
| AU type | aufx (kAudioUnitType_Effect) |
| AU export prefix | OddHarmonicsAU |
| Bundle ID | com.ChrisAudio.OddHarmonics |
| JUCE path | `/Users/chris/src/github/JUCE` |
