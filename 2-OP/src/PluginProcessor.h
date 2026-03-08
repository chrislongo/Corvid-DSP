#pragma once

#include <JuceHeader.h>

#include "plaits/dsp/engine/fm_engine.h"
#include "stmlib/utils/buffer_allocator.h"

//==============================================================================
// Linear ADSR envelope. Attack/decay/release are in seconds; sustain is 0–1.
struct ADSREnv
{
    enum State { Idle, Attack, Decay, Sustain, Release } state = Idle;
    float level         = 0.0f;
    float releaseStart  = 0.0f;  // level captured at note-off, for linear release

    void noteOn()
    {
        // Don't reset level — ramps up from wherever it is (avoids re-trigger click).
        state = Attack;
    }

    void noteOff()
    {
        if (state == Idle) return;
        if (level <= 0.0f) { state = Idle; return; }
        releaseStart = level;
        state = Release;
    }

    bool active() const { return state != Idle; }

    void reset() { state = Idle; level = 0.0f; releaseStart = 0.0f; }

    float processSample (float a, float d, float s, float r, float sr) noexcept
    {
        switch (state)
        {
        case Attack: {
            // Exponential approach to 1.0 — sounds natural and prevents onset clicks.
            const float coeff = (a > 0.0f) ? std::exp (-1.0f / (a * sr)) : 0.0f;
            level = 1.0f - (1.0f - level) * coeff;
            if (level >= 0.999f) { level = 1.0f; state = Decay; }
            break;
        }
        case Decay: {
            if (s >= 1.0f) { state = Sustain; break; }
            const float step = (d > 0.0f) ? (1.0f - s) / (d * sr) : (1.0f - s);
            level = std::max (level - step, s);
            if (level <= s) state = Sustain;
            break;
        }
        case Sustain:
            level = s;
            break;
        case Release: {
            if (releaseStart <= 0.0f) { state = Idle; level = 0.0f; break; }
            const float step = (r > 0.0f) ? releaseStart / (r * sr) : releaseStart;
            level = std::max (level - step, 0.0f);
            if (level <= 0.0f) { level = 0.0f; state = Idle; }
            break;
        }
        case Idle:
            break;
        }
        return level;
    }
};

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

    ADSREnv env_;

    // FM parameter smoothing (20 ms ramp, prevents zipper noise)
    juce::SmoothedValue<float> ratio_smoothed_;
    juce::SmoothedValue<float> index_smoothed_;
    juce::SmoothedValue<float> feedback_smoothed_;
    juce::SmoothedValue<float> sub_smoothed_;
    // Velocity smoothing (5 ms ramp, prevents amplitude pop on retrigger)
    juce::SmoothedValue<float> velocity_smoothed_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwoOpFMAudioProcessor)
};
