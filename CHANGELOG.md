# Changelog

All notable changes to this project are documented here, grouped by plugin and version.

---

## 2-OP

### v1.0 — 2026-03-14
- PING pill now always reads "PING"; fill state indicates mode (outlined = Gate, filled = Ping)
- Move pill to Decay column; reduce width to 38 px; align vertically with section label text
- Extract layout constants for LPG header and pill geometry; eliminate magic numbers

### v1.0 — 2026-03-13
- Replace ADSR with Plaits LPG (vactrol + low-pass gate)
  - New parameters: Decay (0–1 Plaits scale), Color (filter/VCA tilt), Output (dB), Ping toggle
  - Gate mode: LPG follows MIDI gate; Attack knob controls open time; velocity scales amplitude
  - Ping mode: each note-on fires a pitch-proportional impulse; tail decays freely
  - Decay formula matches `plaits/dsp/voice.cc` exactly
  - Removed: Sustain, Release
- Add Shift-key fine-tune mode to all FM sliders
- Refine knob tick marks

### v0.3 — 2026-03-11
- Add tick marks around rotary knobs; widen knob hit bounds

### v0.3 — 2026-03-08
- Enlarge knobs (radius 16 → 26) and centre vertically in LPG + Output section
- Section-box UI redesign: silver/grey rounded-rect panels, section headers
- Widen panel to 545×455 px; add Output knob
- Add VST3 format
- Fix AU manufacturer/plugin code casing (`Cvda`/`Twop`)
- Fix attack click on note re-trigger

### v0.2 — 2026-03-03
- Smooth velocity changes with `SmoothedValue`; clamp output to ±1
- Sample-accurate MIDI event handling (buffer split at event boundaries)
- Add trigger state machine (`RISING_EDGE` → `HIGH` → `LOW`)
- Skewed `NormalisableRange` on ADSR time parameters (exponent 0.3) for musical response

### v0.1 — 2026-03-02
- Initial release: 2-operator FM synthesizer AU instrument
- Wraps Plaits `FMEngine` with sample-rate pitch correction
- Parameters: Ratio, Index, Feedback, Sub, ADSR envelope
- Monophonic, last-note priority; pitch bend ±2 semitones
- Manufacturer code CVDA, plugin code TWOP, bundle `com.CorvidAudio.TwoOpFM`

---

## Dist308

### v0.2 — 2026-03-07
- Compute pre-clip LPF coefficient per-sample for accurate gain tracking at all distortion levels
- Fix stereo channel index bug

### v0.2 — 2026-03-06
- Switch to exponential gain curve (47×–1047×); clean output at zero distortion

### v0.2 — 2026-03-05
- Invert Filter knob direction: CCW = darkest (475 Hz), CW = brightest (22 kHz); default 50%
- Overhaul DSP signal chain for accurate RAT circuit emulation
  - HPF at 180 Hz removes bass bloom before clipping
  - Pre-clip LPF models LM308 GBW (effective 5 MHz, calibrated against real RAT recording)
  - tanh saturation models anti-parallel 1N914 diodes in op-amp feedback
  - Post-clip LPF controlled by Filter knob

### v0.1 — 2026-03-04
- Initial release: ProCo Rat-inspired distortion AU effect
- Parameters: Distortion, Filter, Volume
- Manufacturer code CVDA, plugin code D308, bundle `com.CorvidAudio.Dist308`

---

## Warm

### v0.1 — 2026-03-01
- Initial release: odd-harmonic tanh waveshaper AU effect
- Single Harmonics knob; exponential drive taper
- Manufacturer code CVDA, plugin code ODDH, bundle `com.CorvidAudio.OddHarmonics`
