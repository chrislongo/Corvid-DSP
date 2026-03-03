#pragma once

#include <JuceHeader.h>

#include "plaits/dsp/engine/fm_engine.h"
#include "stmlib/utils/buffer_allocator.h"

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
    double getTailLengthSeconds() const override { return 0.0; }

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
    char engine_buffer_[256];
    stmlib::BufferAllocator allocator_;

    float out_[plaits::kBlockSize];
    float aux_[plaits::kBlockSize];
    bool  already_enveloped_ = false;

    // Voice state
    int   sounding_note_         = -1;   // MIDI note number, -1 = silent
    float velocity_              = 1.0f;
    float pitch_bend_semitones_  = 0.0f;
    float pitch_correction_      = 0.0f; // compensates hardcoded Plaits a0
    bool  gate_                  = false;

    // Parameter smoothing (20 ms ramp)
    juce::SmoothedValue<float> ratio_smoothed_;
    juce::SmoothedValue<float> index_smoothed_;
    juce::SmoothedValue<float> feedback_smoothed_;
    juce::SmoothedValue<float> sub_smoothed_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwoOpFMAudioProcessor)
};
