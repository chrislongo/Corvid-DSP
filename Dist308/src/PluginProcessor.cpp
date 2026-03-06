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
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f,
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
    currentSr   = static_cast<float> (sampleRate);
    twoPiOverSr = juce::MathConstants<float>::twoPi / currentSr;
    piOverSr    = juce::MathConstants<float>::pi    / currentSr;

    // Pre-distortion HPF at ~180 Hz (input AC-coupling model; clears bass bloom before clipping)
    const float hpfK = std::tan (piOverSr * 180.0f);
    hpfCoeff = hpfK / (1.0f + hpfK);

    const float d = apvts.getRawParameterValue ("distortion")->load();
    const float f = apvts.getRawParameterValue ("filter")->load();
    const float v = apvts.getRawParameterValue ("volume")->load();

    // Real RAT: feedback = 47kΩ fixed + 0–1MΩ pot, input = 1kΩ → gain = 47–1047
    const float initGain   = 47.0f + 1000.0f * (d / 100.0f);
    const float initCutoff = 475.0f * std::pow (22000.0f / 475.0f, f / 100.0f);
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

        hpfState[ch]           = 0.0f;
        preClipFilterState[ch] = 0.0f;
        filterState[ch]        = 0.0f;
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


void Dist308AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float d = apvts.getRawParameterValue ("distortion")->load();
    const float f = apvts.getRawParameterValue ("filter")->load();
    const float v = apvts.getRawParameterValue ("volume")->load();

    const float targetInputGain  = 47.0f + 1000.0f * (d / 100.0f);
    const float targetCutoff     = 475.0f * std::pow (22000.0f / 475.0f, f / 100.0f);
    const float normVol          = v / 100.0f;
    const float targetOutputGain = normVol * normVol;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Pre-clip LPF coefficient — computed once per block using the bilinear-transform
    // form (tan-based) rather than the EMA approximation (w/(w+1)).  The EMA form
    // under-estimates the -3dB frequency by up to 40% at mid-high cutoffs, making the
    // plugin sound darker than the real circuit.  Using tan gives exact placement.
    // GBW empirically set to 5 MHz: the LM308's 1 MHz small-signal spec underestimates
    // effective bandwidth when the output spends most of its time in the nonlinear
    // (diode-clamped) region, which is the case for typical guitar signal levels.
    const float preClipFc    = std::min (5.0e6f / targetInputGain, currentSr * 0.45f);
    const float preClipK     = std::tan (piOverSr * preClipFc);
    const float preClipCoeff = preClipK / (1.0f + preClipK);

    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const auto chi = static_cast<size_t> (ch);

        inputGainSmoothed[chi].setTargetValue (targetInputGain);
        filterCutoffSmoothed[chi].setTargetValue (targetCutoff);
        outputGainSmoothed[chi].setTargetValue (targetOutputGain);

        float* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = inputGainSmoothed[chi].getNextValue();

            // 1. Pre-distortion HPF at ~100 Hz (models input AC-coupling cap).
            //    Removes sub-bass before clipping so it doesn't occupy headroom and
            //    mask the midrange harmonics.
            hpfState[ch] += hpfCoeff * (data[i] - hpfState[ch]);
            float x = data[i] - hpfState[ch];

            // 2. LM308 bandwidth model: one-pole LPF, fc = GBW/gain (~1 MHz GBW).
            //    Coefficient is pre-computed per block (accurate bilinear form).
            preClipFilterState[ch] = preClipCoeff * x
                                     + (1.0f - preClipCoeff) * preClipFilterState[ch];

            // 3. Amplify + diode saturation.
            //    tanh models anti-parallel 1N914s in op-amp feedback.
            x = std::tanh (preClipFilterState[ch] * gain);

            // 4. Post-clip one-pole LPF (FILTER knob, 475 Hz → 22 kHz)
            const float fc    = filterCutoffSmoothed[chi].getNextValue();
            const float w     = twoPiOverSr * fc;
            const float coeff = w / (w + 1.0f);
            filterState[ch]   = coeff * x + (1.0f - coeff) * filterState[ch];

            // 5. Output gain (VOLUME)
            data[i] = filterState[ch] * outputGainSmoothed[chi].getNextValue();
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
