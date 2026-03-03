#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

// The Plaits engine's a0 constant is derived from its hardware sample rate
// (47872.34 Hz). We compensate by adding a semitone offset to every note
// so that NoteToFrequency() returns the correct pitch at any host sample rate.
static constexpr float kPlaitsHardwareSampleRate = 47872.34f;

//==============================================================================
TwoOpFMAudioProcessor::TwoOpFMAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

TwoOpFMAudioProcessor::~TwoOpFMAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TwoOpFMAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // FM parameters
    layout.add (std::make_unique<APF> ("ratio",    "Ratio",    0.0f, 1.0f,  0.5f));
    layout.add (std::make_unique<APF> ("index",    "Index",    0.0f, 1.0f,  0.3f));
    layout.add (std::make_unique<APF> ("feedback", "Feedback", 0.0f, 1.0f,  0.5f));
    layout.add (std::make_unique<APF> ("sub",      "Sub",      0.0f, 1.0f,  0.0f));

    // Envelope parameters (times in seconds)
    layout.add (std::make_unique<APF> ("attack",   "Attack",   0.001f, 2.0f, 0.008f));
    layout.add (std::make_unique<APF> ("decay",    "Decay",    0.001f, 4.0f, 0.001f));
    layout.add (std::make_unique<APF> ("sustain",  "Sustain",  0.0f,   1.0f, 1.0f));
    layout.add (std::make_unique<APF> ("release",  "Release",  0.001f, 4.0f, 0.001f));

    return layout;
}

//==============================================================================
void TwoOpFMAudioProcessor::prepareToPlay (double sampleRate, int)
{
    allocator_.Init (engine_buffer_, sizeof (engine_buffer_));
    engine_.Init (&allocator_);

    pitch_correction_ = 12.0f * std::log2f (kPlaitsHardwareSampleRate / (float) sampleRate);

    env_.reset();

    const double ramp = 0.02;
    ratio_smoothed_   .reset (sampleRate, ramp);
    index_smoothed_   .reset (sampleRate, ramp);
    feedback_smoothed_.reset (sampleRate, ramp);
    sub_smoothed_     .reset (sampleRate, ramp);

    ratio_smoothed_   .setCurrentAndTargetValue (*apvts.getRawParameterValue ("ratio"));
    index_smoothed_   .setCurrentAndTargetValue (*apvts.getRawParameterValue ("index"));
    feedback_smoothed_.setCurrentAndTargetValue (*apvts.getRawParameterValue ("feedback"));
    sub_smoothed_     .setCurrentAndTargetValue (*apvts.getRawParameterValue ("sub"));
}

//==============================================================================
void TwoOpFMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Process MIDI at block start (last-note priority).
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            sounding_note_ = msg.getNoteNumber();
            velocity_      = msg.getVelocity() / 127.0f;
            gate_          = true;
            env_.noteOn();
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == sounding_note_)
            {
                gate_ = false;
                env_.noteOff();
                // Keep sounding_note_ set — engine needs a pitch during release.
            }
        }
        else if (msg.isPitchWheel())
        {
            pitch_bend_semitones_ = (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        }
    }

    // Nothing to render if gate closed and envelope fully decayed.
    if (! gate_ && ! env_.active())
        return;

    // Read ADSR params once per block (envelope itself provides smooth amplitude).
    const float atk = *apvts.getRawParameterValue ("attack");
    const float dcy = *apvts.getRawParameterValue ("decay");
    const float sus = *apvts.getRawParameterValue ("sustain");
    const float rel = *apvts.getRawParameterValue ("release");
    const float sr  = (float) getSampleRate();

    // Update FM smoother targets.
    ratio_smoothed_   .setTargetValue (*apvts.getRawParameterValue ("ratio"));
    index_smoothed_   .setTargetValue (*apvts.getRawParameterValue ("index"));
    feedback_smoothed_.setTargetValue (*apvts.getRawParameterValue ("feedback"));
    sub_smoothed_     .setTargetValue (*apvts.getRawParameterValue ("sub"));

    const int total    = buffer.getNumSamples();
    const int numCh    = buffer.getNumChannels();
    float* left  = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int offset = 0; offset < total; offset += (int) plaits::kBlockSize)
    {
        const int chunk = std::min ((int) plaits::kBlockSize, total - offset);

        // Advance FM smoothers by exactly chunk samples.
        float ratio_val = 0.0f, index_val = 0.0f, feedback_val = 0.0f, sub_val = 0.0f;
        for (int i = 0; i < chunk; ++i)
        {
            ratio_val    = ratio_smoothed_   .getNextValue();
            index_val    = index_smoothed_   .getNextValue();
            feedback_val = feedback_smoothed_.getNextValue();
            sub_val      = sub_smoothed_     .getNextValue();
        }

        plaits::EngineParameters params;
        params.note      = (float) sounding_note_ + pitch_bend_semitones_ + pitch_correction_;
        params.harmonics = ratio_val;
        params.timbre    = index_val;
        params.morph     = feedback_val;
        params.accent    = velocity_;
        params.trigger   = plaits::TRIGGER_HIGH;

        engine_.Render (params, out_, aux_, (size_t) chunk, &already_enveloped_);

        for (int i = 0; i < chunk; ++i)
        {
            const float env_amp = env_.processSample (atk, dcy, sus, rel, sr);
            const float s = (out_[i] + sub_val * aux_[i]) * env_amp;
            left [offset + i] = s;
            if (right != nullptr)
                right[offset + i] = s;
        }
    }
}

//==============================================================================
void TwoOpFMAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void TwoOpFMAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* TwoOpFMAudioProcessor::createEditor()
{
    return new TwoOpFMAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TwoOpFMAudioProcessor();
}
