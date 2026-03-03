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

*Key fixes applied during first build:*
- Added `TEST=1` compile definition to bypass ARM VFP assembly in `stmlib/dsp/dsp.h`
- Added `${EURORACK}/stmlib/dsp/units.cc` to sources (pitch ratio LUTs required by linker)
- Fixed `getSliderThumbRadius()` → returns 5 (= kThumbH/2) to prevent thumb clipping at extremes
- Added linear ADSR (`ADSREnv`) and 4 ADSR parameters to eliminate note-on click

## 3. Install & sign

- [x] Copy `.component` to `~/Library/Audio/Plug-Ins/Components/`
- [x] Ad-hoc codesign the component

## 4. AU validation

- [x] `auval -v aumu TWOP CVDA` passes with no errors

## 5. UI redesign — 4 sliders + 4 knobs

- [ ] Add `ADSRKnobLookAndFeel` class (rotary, matte-black, white indicator line)
- [ ] Resize panel to 300 × 340
- [ ] Replace 8-slider layout with 4 FM sliders (top) + 4 ADSR rotary knobs (bottom)
- [ ] Add horizontal separator between sections
- [ ] Build, install, sign
- [ ] `auval -v aumu TWOP CVDA` still passes

## 6. DSP correctness

- [ ] Play MIDI notes across full pitch range — confirm tuning is accurate at host sample rate
- [ ] Note on/off — no stuck notes, clean release
- [ ] Rapid note changes — last-note priority works, no glitches

## 7. Parameter sweep

- [ ] **Ratio** — audible pitch steps in modulator; snaps through quantized intervals
- [ ] **Index** — modulation depth sweeps cleanly from sine to full FM
- [ ] **Feedback** — 0→0.5 adds phase feedback; 0.5→1 adds self-feedback; no artifacts
- [ ] **Sub** — audibly blends sub oscillator; at 0 = silent sub, at 1 = full blend
- [ ] **Attack / Decay / Release** — no audible click at transitions; times feel accurate
- [ ] **Sustain** — level holds correctly while key is held

## 8. UI check

- [ ] Plugin title "2-OP" appears top-centre
- [ ] FM sliders at columns 55, 118, 182, 245 — track, thumb, and ticks match mockup v2
- [ ] ADSR knobs at same columns, centre y=285 — indicator angle matches parameter value
- [ ] Section separator visible between slider and knob rows
- [ ] Labels (RATIO/INDEX/FDBK/SUB, ATK/DCY/SUS/REL) and value readouts correct
- [ ] Value display updates live as controls move
- [ ] Background: silver-grey gradient, flat — no bevels or shadows

## 9. State persistence

- [ ] Move controls, close and reopen plugin — values restored
- [ ] Standalone: quit and relaunch — values restored

## 10. Create CLAUDE.md

- [ ] Write `2-OP/CLAUDE.md` with build commands, plugin identity, auval invocation, and architecture notes
