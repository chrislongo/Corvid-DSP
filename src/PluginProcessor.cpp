#include "PluginProcessor.h"
#include "PluginEditor.h"

OddHarmonicsAudioProcessor::OddHarmonicsAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

OddHarmonicsAudioProcessor::~OddHarmonicsAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
OddHarmonicsAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Log (audio) taper: centre of knob = 25% of range, mimicking a Rat-style C-curve pot
    juce::NormalisableRange<float> range (0.0f, 100.0f, 0.01f);
    range.setSkewForCentre (25.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "amount", 1 },
        "Amount",
        range,
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")
    ));

    return layout;
}

void OddHarmonicsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    for (auto& smoother : driveSmoothed)
    {
        smoother.reset (sampleRate, 0.05);   // 50ms ramp
        smoother.setCurrentAndTargetValue (0.5f);
    }
}

void OddHarmonicsAudioProcessor::releaseResources() {}

bool OddHarmonicsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void OddHarmonicsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float amount     = apvts.getRawParameterValue ("amount")->load();
    const float normAmount = amount / 100.0f;

    // Rat-style drive: 0.5 at 0% (subtle saturation) → 20.0 at 100% (extreme)
    const float targetDrive = 0.5f * std::pow (40.0f, normAmount);

    // Gentle makeup gain: about -1 dB at max drive
    const float makeupGain  = 1.0f - normAmount * 0.109f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const auto chIdx = static_cast<size_t> (ch);
        driveSmoothed[chIdx].setTargetValue (targetDrive);
        float* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drive      = driveSmoothed[chIdx].getNextValue();
            const float tanhDrive  = std::tanh (drive);

            // Avoid divide-by-zero at near-zero drive
            const float shaped = (tanhDrive > 1e-6f)
                ? std::tanh (drive * data[i]) / tanhDrive
                : data[i];

            data[i] = shaped * makeupGain;
        }
    }
}

juce::AudioProcessorEditor* OddHarmonicsAudioProcessor::createEditor()
{
    return new OddHarmonicsAudioProcessorEditor (*this);
}

void OddHarmonicsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OddHarmonicsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OddHarmonicsAudioProcessor();
}
