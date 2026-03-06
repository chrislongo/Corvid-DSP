#pragma once

#include <JuceHeader.h>

class Dist308AudioProcessor : public juce::AudioProcessor
{
public:
    Dist308AudioProcessor();
    ~Dist308AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dist308"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    std::array<juce::SmoothedValue<float>, 2> inputGainSmoothed;
    std::array<juce::SmoothedValue<float>, 2> filterCutoffSmoothed;
    std::array<juce::SmoothedValue<float>, 2> outputGainSmoothed;

    float hpfState[2]        { 0.0f, 0.0f };  // pre-distortion HPF (~100 Hz)
    float preClipFilterState[2] { 0.0f, 0.0f };  // LM308 GBW model
    float filterState[2]     { 0.0f, 0.0f };
    float twoPiOverSr = 0.0f;
    float piOverSr    = 0.0f;
    float currentSr   = 44100.0f;
    float hpfCoeff    = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dist308AudioProcessor)
};
