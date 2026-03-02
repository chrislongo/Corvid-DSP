#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class MetallicKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetallicKnobLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    juce::Label* createSliderTextBox (juce::Slider& slider) override;
};

//==============================================================================
class OddHarmonicsAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit OddHarmonicsAudioProcessorEditor (OddHarmonicsAudioProcessor&);
    ~OddHarmonicsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MetallicKnobLookAndFeel blackKnobLAF;

    juce::Slider harmonicsKnob;
    juce::Label  harmonicsLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment harmonicsAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OddHarmonicsAudioProcessorEditor)
};
