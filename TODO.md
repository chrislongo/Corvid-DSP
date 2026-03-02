# Warm Plugin — TODO

## Setup
- [x] Create REQUIREMENTS.md
- [x] Create DESIGN.md
- [x] Create CMakeLists.txt
- [x] Create src/PluginProcessor.h
- [x] Create src/PluginProcessor.cpp
- [x] Create src/PluginEditor.h
- [x] Create src/PluginEditor.cpp

## Build
- [x] Run `cmake -B build -G Ninja -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`
- [x] Run `cmake --build build --config Release`
- [x] Verify no compiler errors or warnings

## Install
- [x] Copy `.component` to `~/Library/Audio/Plug-Ins/Components/`
- [x] Ad-hoc codesign the component (dev only)
- [x] Run `killall -9 AudioComponentRegistrar` to refresh AU cache

## Validation
- [x] Run `auval -v aufx ODDH CHRS` — must pass all tests
- [ ] Load in Logic Pro or GarageBand
- [ ] Verify HARMONICS knob 0→100% adds progressive saturation
- [ ] Verify no clicks or zipper noise during knob movement
- [ ] Test preset save/load (save state, reload session, confirm restored)
- [ ] Test mono and stereo inputs

## Polish
- [x] Test universal binary: `lipo -info Warm.component/Contents/MacOS/Warm`
- [x] Profile CPU usage at max drive setting
- [ ] Verify plugin name shows correctly in DAW plugin menu
