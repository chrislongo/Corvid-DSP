# Warm — Plugin Requirements

## Plugin Identity
- **Name**: Warm
- **Type**: AU Effect (kAudioUnitType_Effect)
- **Manufacturer**: CorvidAudio (code: CVDA)
- **Plugin Code**: ODDH
- **Version**: 1.0.0

## Formats
- Audio Unit (AU)
- Standalone application

## DSP
- Adds odd harmonics to audio via tanh waveshaping
- Algorithm: `y = tanh(drive * x) / tanh(drive)`
- Drive mapped exponentially from 0.5 (0%) to 20.0 (100%): `drive = 0.5 × 40^normAmount`
- Gentle makeup gain to compensate for level reduction at high drive

## Parameters
| Name   | ID     | Range   | Default | Unit | Taper |
|--------|--------|---------|---------|------|-------|
| Amount | amount | 0–100   | 0       | %    | Log (audio), centre at 25% |

## I/O
- Mono or stereo passthrough
- No MIDI input required

## UI
- Size: 300×220 px
- Aesthetic: light silver-grey body, dark footer bar (Valhalla-inspired)
- Controls: single large rotary "HARMONICS" knob (matte black, white indicator), centered in the UI
- No plugin name in the interface
- Text box: shows current value as "X.X %"
- Footer: None

## Build
- Build system: CMake 3.22+
- Target platform: macOS 11.0+
- Architectures: arm64 + x86_64 (universal binary)
- JUCE version: latest at /Users/chris/src/github/JUCE

## State
- Preset save/load via AudioProcessorValueTreeState XML serialization
- Standard `getStateInformation` / `setStateInformation` API
