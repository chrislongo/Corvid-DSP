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

    // LPG parameters
    using Range = juce::NormalisableRange<float>;
    layout.add (std::make_unique<APF> ("attack",  "Attack",  Range (0.005f, 2.0f, 0.0f, 0.3f), 0.010f));
    layout.add (std::make_unique<APF> ("decay",   "Decay",   0.0f, 1.0f, 0.5f));
    layout.add (std::make_unique<APF> ("color",  "Color",  0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> ("ping", "Ping Mode", false));

    // Output level in dB
    layout.add (std::make_unique<APF> ("output", "Output", -40.0f, 0.0f, -12.0f));

    return layout;
}

//==============================================================================
void TwoOpFMAudioProcessor::prepareToPlay (double sampleRate, int)
{
    allocator_.Init (engine_buffer_, sizeof (engine_buffer_));
    engine_.Init (&allocator_);

    pitch_correction_ = 12.0f * std::log2f (kPlaitsHardwareSampleRate / (float) sampleRate);

    lpg_envelope_.Init();
    lpg_.Init();
    lpg_gate_level_  = 0.0f;
    lpg_gate_target_ = 0.0f;
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

    // Read LPG params once per block.
    const float atk    = *apvts.getRawParameterValue ("attack");
    const float decay  = *apvts.getRawParameterValue ("decay");
    const float colour = *apvts.getRawParameterValue ("color");
    const bool pingMode = *apvts.getRawParameterValue ("ping") > 0.5f;
    const float sr     = (float) getSampleRate();

    const float outputGain = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue ("output")->load());

    // Plaits LPG decay formula (same as voice.cc).
    const float short_decay = (200.0f * (float) plaits::kBlockSize) / sr
                              * std::exp2f (-96.0f * decay / 12.0f);
    const float decay_tail  = (20.0f  * (float) plaits::kBlockSize) / sr
                              * std::exp2f ((-72.0f * decay + 12.0f * colour) / 12.0f)
                              - short_decay;

    // Attack ramp rate: how much lpg_gate_level_ advances per 12-sample block.
    const float blockRate  = sr / (float) plaits::kBlockSize;
    const float atk_step   = (atk > 0.0f) ? (1.0f / (atk * blockRate)) : 1.0f;

    // Update FM smoother targets.
    ratio_smoothed_   .setTargetValue (*apvts.getRawParameterValue ("ratio"));
    index_smoothed_   .setTargetValue (*apvts.getRawParameterValue ("index"));
    feedback_smoothed_.setTargetValue (*apvts.getRawParameterValue ("feedback"));
    sub_smoothed_     .setTargetValue (*apvts.getRawParameterValue ("sub"));

    const int total = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    float* left  = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // Returns true while the LPG is still producing audible output.
    auto lpgActive = [&] { return gate_ || lpg_envelope_.gain() > 0.001f; };

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

            if (pingMode)
            {
                // Pitch-proportional attack (higher notes ping faster, like Plaits).
                const float ping_attack = std::exp2f ((float) (sounding_note_ - 69) / 12.0f)
                                          * 440.0f / sr * (float) plaits::kBlockSize * 2.0f;
                lpg_envelope_.ProcessPing (ping_attack, short_decay, decay_tail, colour);
            }
            else
            {
                // Ramp gate level toward target (attack) or drop instantly (note-off).
                if (lpg_gate_target_ > lpg_gate_level_)
                    lpg_gate_level_ = std::min (lpg_gate_level_ + atk_step, lpg_gate_target_);
                else
                    lpg_gate_level_ = lpg_gate_target_;

                lpg_envelope_.ProcessLP (lpg_gate_level_, short_decay, decay_tail, colour);
            }

            // Blend sub into main (in-place), apply 0.6 gain to match Plaits voice level.
            for (int i = 0; i < chunk; ++i)
                out_[i] = (out_[i] + sub_val * aux_[i]) * 0.6f;

            // Apply LPG: simultaneous VCA + VCF, interpolating gain over the chunk.
            lpg_.Process (lpg_envelope_.gain(),
                          lpg_envelope_.frequency(),
                          lpg_envelope_.hf_bleed(),
                          out_, (size_t) chunk);

            for (int i = 0; i < chunk; ++i)
            {
                const float s = std::clamp (out_[i], -1.0f, 1.0f) * outputGain;
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

        if (eventPos > cursor && lpgActive())
            renderSegment (cursor, eventPos);
        cursor = eventPos;

        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            sounding_note_   = msg.getNoteNumber();
            velocity_        = msg.getVelocity() / 127.0f;
            gate_            = true;
            trigger_         = plaits::TRIGGER_RISING_EDGE;
            lpg_gate_target_ = velocity_;
            if (pingMode)
                lpg_envelope_.Trigger();
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == sounding_note_)
            {
                gate_            = false;
                trigger_         = plaits::TRIGGER_LOW;
                lpg_gate_target_ = 0.0f;
                // Keep sounding_note_ set — engine needs a pitch during LPG tail.
            }
        }
        else if (msg.isPitchWheel())
        {
            pitch_bend_semitones_ = (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        }
    }

    // Render remaining samples after the last MIDI event.
    if (cursor < total && lpgActive())
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
