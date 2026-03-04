#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class BlackKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlackKnobLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;
};

//==============================================================================
class Dist308AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit Dist308AudioProcessorEditor (Dist308AudioProcessor&);
    ~Dist308AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BlackKnobLookAndFeel blackKnobLAF;

    juce::Slider distKnob, filterKnob, volumeKnob;
    juce::Label  distLabel, filterLabel, volumeLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment distAttachment,
                                                         filterAttachment,
                                                         volumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dist308AudioProcessorEditor)
};
