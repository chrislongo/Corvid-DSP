# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Workspace layout

This is a multi-project DSP plugin workspace. Each plugin lives in its own subdirectory with its own `CMakeLists.txt`, `CLAUDE.md`, and `src/`.

| Dir | Plugin | Type |
|-----|--------|------|
| `Warm/` | Odd-harmonic tanh waveshaper | AU Effect |
| `2-OP/` | 2-operator FM synthesizer | AU Instrument |

Read the project-specific `CLAUDE.md` before working inside a project folder.

## Toolchain

- **cmake**: `/opt/homebrew/bin/cmake` — Xcode is **not** installed; always use `-G Ninja`
- **Compiler**: `xcrun -f clang` / `xcrun -f clang++` (Apple Clang via Xcode CLI tools)
- **JUCE**: `/Users/chris/src/github/JUCE` (added via `add_subdirectory` in each project)
- **eurorack/stmlib**: `/Users/chris/src/github/eurorack` (used by 2-OP)

## Common build pattern

Each project follows the same steps. Run all commands from inside the project directory (e.g. `Warm/`).

```bash
# Configure (once, or after CMakeLists changes)
/opt/homebrew/bin/cmake -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=$(xcrun -f clang) \
    -DCMAKE_CXX_COMPILER=$(xcrun -f clang++)

# Build
/opt/homebrew/bin/cmake --build build --config Release
```

After building, copy the `.component` to `~/Library/Audio/Plug-Ins/Components/` and ad-hoc sign it before running `auval`. See the project `CLAUDE.md` for exact paths and AU codes.

## JUCE conventions (apply to all projects)

- `juce_generate_juce_header(<Target>)` is required — without it `<JuceHeader.h>` is missing.
- Standalone format needs `juce::juce_audio_utils` and `juce::juce_audio_devices` linked.
- Use `juce::Font(juce::FontOptions().withHeight(n))` — the `Font(float)` constructor is deprecated.
- `AudioProcessorEditor` already has a `processor` member; don't shadow it in subclasses.
- Index `std::array<SmoothedValue>` with `static_cast<size_t>(ch)` to avoid `-Wsign-conversion`.
- APVTS lives in the processor (public) and is accessed by the editor via `SliderAttachment` — no manual sync needed.

## Testing

There are no automated tests. `auval` is the primary correctness check for every plugin. Each project `CLAUDE.md` lists the exact `auval` invocation for that plugin's AU codes.

## Design language

All plugins share the same visual style:
- **Panel**: silver-grey `#d8d8d8`, subtle top→bottom gradient overlay (`0x18ffffff` → `0x18000000`), completely flat — no bevels or shadows
- **Controls**: matte-black (`#111111`) with white indicator lines/hairlines
- **Labels**: 10–13pt bold, dark grey, all-caps where used for parameter names
- Each project's `design/` folder contains an SVG mockup of the UI
