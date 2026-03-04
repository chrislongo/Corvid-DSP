#include "PluginProcessor.h"
#include "PluginEditor.h"

Dist308AudioProcessor::Dist308AudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

Dist308AudioProcessor::~Dist308AudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
Dist308AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::NormalisableRange<float> distRange (0.0f, 100.0f, 0.01f);
    distRange.setSkewForCentre (20.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "distortion", 1 }, "Distortion", distRange, 35.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "filter", 1 }, "Filter",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    juce::NormalisableRange<float> volRange (0.0f, 100.0f, 0.01f);
    volRange.setSkewForCentre (35.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume", volRange, 75.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    return layout;
}

void Dist308AudioProcessor::prepareToPlay (double sampleRate, int)
{
    twoPiOverSr = juce::MathConstants<float>::twoPi / static_cast<float> (sampleRate);

    const float d = apvts.getRawParameterValue ("distortion")->load();
    const float f = apvts.getRawParameterValue ("filter")->load();
    const float v = apvts.getRawParameterValue ("volume")->load();

    const float initGain   = std::pow (10.0f, d * 3.35f / 100.0f);
    const float initCutoff = 22000.0f * std::pow (475.0f / 22000.0f, f / 100.0f);
    const float normV      = v / 100.0f;
    const float initOutput = normV * normV;

    for (size_t ch = 0; ch < 2; ++ch)
    {
        inputGainSmoothed[ch].reset (sampleRate, 0.05);
        inputGainSmoothed[ch].setCurrentAndTargetValue (initGain);

        filterCutoffSmoothed[ch].reset (sampleRate, 0.02);
        filterCutoffSmoothed[ch].setCurrentAndTargetValue (initCutoff);

        outputGainSmoothed[ch].reset (sampleRate, 0.02);
        outputGainSmoothed[ch].setCurrentAndTargetValue (initOutput);

        filterState[ch] = 0.0f;
    }
}

void Dist308AudioProcessor::releaseResources() {}

bool Dist308AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

float Dist308AudioProcessor::clip (float x) noexcept
{
    constexpr float T = 0.9f;
    const float ax = std::abs (x);
    if (ax <= T)    return x;
    if (ax >= 1.0f) return std::copysign (1.0f, x);
    const float t = (ax - T) / (1.0f - T);
    return std::copysign (T + (1.0f - T) * (3.0f * t * t - 2.0f * t * t * t), x);
}

void Dist308AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float d = apvts.getRawParameterValue ("distortion")->load();
    const float f = apvts.getRawParameterValue ("filter")->load();
    const float v = apvts.getRawParameterValue ("volume")->load();

    const float targetInputGain  = std::pow (10.0f, d * 3.35f / 100.0f);
    const float targetCutoff     = 22000.0f * std::pow (475.0f / 22000.0f, f / 100.0f);
    const float normVol          = v / 100.0f;
    const float targetOutputGain = normVol * normVol;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const auto chi = static_cast<size_t> (ch);

        inputGainSmoothed[chi].setTargetValue (targetInputGain);
        filterCutoffSmoothed[chi].setTargetValue (targetCutoff);
        outputGainSmoothed[chi].setTargetValue (targetOutputGain);

        float* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            // 1. Input gain (DISTORTION)
            float x = data[i] * inputGainSmoothed[chi].getNextValue();

            // 2. Soft-knee clipper (T=0.9, models 1N914 diodes)
            x = clip (x);

            // 3. One-pole LPF (FILTER)
            const float fc    = filterCutoffSmoothed[chi].getNextValue();
            const float w     = twoPiOverSr * fc;
            const float coeff = w / (w + 1.0f);
            filterState[ch]   = coeff * x + (1.0f - coeff) * filterState[ch];
            x = filterState[ch];

            // 4. Output gain (VOLUME)
            data[i] = x * outputGainSmoothed[chi].getNextValue();
        }
    }
}

juce::AudioProcessorEditor* Dist308AudioProcessor::createEditor()
{
    return new Dist308AudioProcessorEditor (*this);
}

void Dist308AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Dist308AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Dist308AudioProcessor();
}
