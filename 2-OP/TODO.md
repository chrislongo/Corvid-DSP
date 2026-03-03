# 2-OP — Build & Test TODO

Tasks in order. Each depends on the previous completing cleanly.

---

## 1. Scaffold

- [x] Create `CMakeLists.txt` (per DESIGN.md)
- [x] Create `src/PluginProcessor.h`
- [x] Create `src/PluginProcessor.cpp`
- [x] Create `src/PluginEditor.h`
- [x] Create `src/PluginEditor.cpp`

## 2. First build

- [x] Run CMake configure — fix any missing header / path errors
- [x] Run CMake build — fix any compile errors
- [x] Confirm `.component` artifact exists under `build/TwoOpFM_artefacts/`

## 3. Install & sign

- [x] Copy `.component` to `~/Library/Audio/Plug-Ins/Components/`
- [x] Ad-hoc codesign the component

## 4. AU validation

- [x] `auval -v aumu TWOP CVDA` passes with no errors

## 5. DSP correctness

- [ ] Play MIDI notes across full pitch range — confirm tuning is accurate at host sample rate
- [ ] Note on/off — no stuck notes, clean release
- [ ] Rapid note changes — last-note priority works, no glitches

## 6. Parameter sweep

- [ ] **Ratio** — audible pitch steps in modulator; snaps through quantized intervals
- [ ] **Index** — modulation depth sweeps cleanly from sine to full FM
- [ ] **Feedback** — 0→0.5 adds phase feedback; 0.5→1 adds self-feedback; no artifacts
- [ ] **Sub** — audibly blends sub oscillator; at 0 = silent sub, at 1 = full blend

## 7. UI check

- [ ] Plugin title "2-OP" appears top-centre
- [ ] Four sliders at correct column positions (55, 118, 182, 245)
- [ ] Track, thumb, and tick marks match SVG mockup
- [ ] Labels RATIO / INDEX / FDBK / SUB below sliders
- [ ] Value display updates as slider moves
- [ ] Background gradient correct (silver-grey, no bevels)

## 8. State persistence

- [ ] Move sliders, close and reopen plugin — values restored
- [ ] Standalone: quit and relaunch — values restored

## 9. Create CLAUDE.md

- [ ] Write `2-OP/CLAUDE.md` with build commands, plugin identity, and architecture notes
