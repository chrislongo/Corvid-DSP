cmake_minimum_required(VERSION 3.22)
project(TwoOpFM VERSION 0.1.0)

add_subdirectory(/Users/chris/src/github/JUCE JUCE)

set(EURORACK /Users/chris/src/github/eurorack)

juce_add_plugin(TwoOpFM
    COMPANY_NAME            "CorvidAudio"
    PLUGIN_MANUFACTURER_CODE CVDA
    PLUGIN_CODE             TWOP
    FORMATS                 AU Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    AU_EXPORT_PREFIX        TwoOpFMAU
    BUNDLE_ID               com.CorvidAudio.TwoOpFM
    PRODUCT_NAME            "2-OP"
)

juce_generate_juce_header(TwoOpFM)

target_sources(TwoOpFM PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    ${EURORACK}/plaits/dsp/engine/fm_engine.cc
    ${EURORACK}/plaits/resources.cc
    ${EURORACK}/stmlib/dsp/units.cc
)

target_include_directories(TwoOpFM PRIVATE ${EURORACK})

target_compile_definitions(TwoOpFM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # Bypass ARM-only VFP assembly in stmlib/dsp/dsp.h (Sqrt/Clip16/ClipU16)
    TEST=1
)

target_link_libraries(TwoOpFM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_plugin_client
)
