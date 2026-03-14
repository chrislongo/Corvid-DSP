#pragma once

#include <JuceHeader.h>

#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/envelope.h"
#include "plaits/dsp/fx/low_pass_gate.h"
#include "stmlib/utils/buffer_allocator.h"

//==============================================================================
class TwoOpFMAudioProcessor : public juce::AudioProcessor
{
public:
    TwoOpFMAudioProcessor();
    ~TwoOpFMAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "2-OP"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int size) override;

    // Public so the editor can attach SliderAttachments
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    plaits::FMEngine engine_;
    char engine_buffer_[1024];
    stmlib::BufferAllocator allocator_;

    float out_[plaits::kBlockSize];
    float aux_[plaits::kBlockSize];
    bool  already_enveloped_ = false;

    // Voice state
    int   sounding_note_        = 60;    // MIDI note; kept after note-off for release pitch
    float velocity_             = 1.0f;
    float pitch_bend_semitones_ = 0.0f;
    float pitch_correction_     = 0.0f;  // compensates hardcoded Plaits a0
    bool  gate_                 = false; // true while key is physically held
    int   trigger_              = plaits::TRIGGER_LOW;  // RISING_EDGE → HIGH → LOW

    // LPG
    plaits::LPGEnvelope lpg_envelope_;
    plaits::LowPassGate lpg_;
    float lpg_gate_level_  = 0.0f;  // current ramped level input to ProcessLP
    float lpg_gate_target_ = 0.0f;  // target: velocity_ on note-on, 0 on note-off

    // FM parameter smoothing (20 ms ramp, prevents zipper noise)
    juce::SmoothedValue<float> ratio_smoothed_;
    juce::SmoothedValue<float> index_smoothed_;
    juce::SmoothedValue<float> feedback_smoothed_;
    juce::SmoothedValue<float> sub_smoothed_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwoOpFMAudioProcessor)
};
