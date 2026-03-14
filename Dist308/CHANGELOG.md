# Dist308 Changelog

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
