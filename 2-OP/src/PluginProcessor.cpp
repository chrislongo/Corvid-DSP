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

    // Envelope parameters (times in seconds, skewed so fine control is at the short end)
    using Range = juce::NormalisableRange<float>;
    layout.add (std::make_unique<APF> ("attack",  "Attack",  Range (0.001f, 2.0f, 0.0f, 0.3f), 0.008f));
    layout.add (std::make_unique<APF> ("decay",   "Decay",   Range (0.001f, 4.0f, 0.0f, 0.3f), 0.001f));
    layout.add (std::make_unique<APF> ("sustain", "Sustain", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<APF> ("release", "Release", Range (0.001f, 4.0f, 0.0f, 0.3f), 0.001f));

    return layout;
}

//==============================================================================
void TwoOpFMAudioProcessor::prepareToPlay (double sampleRate, int)
{
    allocator_.Init (engine_buffer_, sizeof (engine_buffer_));
    engine_.Init (&allocator_);

    pitch_correction_ = 12.0f * std::log2f (kPlaitsHardwareSampleRate / (float) sampleRate);

    env_.reset();
    trigger_ = plaits::TRIGGER_LOW;

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

    const int total = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    float* left  = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // Renders samples [start, end) using current voice state.
    auto renderSegment = [&] (int start, int end)
    {
        for (int offset = start; offset < end; offset += (int) plaits::kBlockSize)
        {
            const int chunk = std::min ((int) plaits::kBlockSize, end - offset);

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
            params.trigger   = trigger_;

            engine_.Render (params, out_, aux_, (size_t) chunk, &already_enveloped_);

            // Advance trigger state: rising edge lasts only for the first render chunk.
            if (trigger_ == plaits::TRIGGER_RISING_EDGE)
                trigger_ = plaits::TRIGGER_HIGH;

            for (int i = 0; i < chunk; ++i)
            {
                const float env_amp = env_.processSample (atk, dcy, sus, rel, sr);
                // 0.6f matches the out_gain/aux_gain Plaits applies to FMEngine in its voice.
                const float s = (out_[i] * 0.6f + sub_val * aux_[i] * 0.6f) * env_amp * velocity_;
                left [offset + i] = s;
                if (right != nullptr)
                    right[offset + i] = s;
            }
        }
    };

    // Interleave MIDI events with rendering at their exact sample positions.
    int cursor = 0;
    for (const auto meta : midi)
    {
        const int eventPos = juce::jlimit (0, total, meta.samplePosition);

        if (eventPos > cursor && (gate_ || env_.active()))
            renderSegment (cursor, eventPos);
        cursor = eventPos;

        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            sounding_note_ = msg.getNoteNumber();
            velocity_      = msg.getVelocity() / 127.0f;
            gate_          = true;
            trigger_       = plaits::TRIGGER_RISING_EDGE;
            env_.noteOn();
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == sounding_note_)
            {
                gate_    = false;
                trigger_ = plaits::TRIGGER_LOW;
                env_.noteOff();
                // Keep sounding_note_ set — engine needs a pitch during release.
            }
        }
        else if (msg.isPitchWheel())
        {
            pitch_bend_semitones_ = (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        }
    }

    // Render remaining samples after the last MIDI event.
    if (cursor < total && (gate_ || env_.active()))
        renderSegment (cursor, total);
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
