#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom LookAndFeel for the vertical sliders.
// Draws a flat matte-black thumb (28×10 px) with a white hairline, a narrow
// dark track (4 px), and flanking tick marks — TR-808 drum machine aesthetic.
class FMSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FMSliderLookAndFeel();

    void drawLinearSlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           juce::Slider::SliderStyle,
                           juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider&) override { return 0; }
};

//==============================================================================
class TwoOpFMAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TwoOpFMAudioProcessorEditor (TwoOpFMAudioProcessor&);
    ~TwoOpFMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FMSliderLookAndFeel sliderLAF_;

    juce::Slider ratioSlider_, indexSlider_, feedbackSlider_, subSlider_;
    juce::Label  ratioLabel_,  indexLabel_,  feedbackLabel_,  subLabel_;
    juce::Label  ratioValue_,  indexValue_,  feedbackValue_,  subValue_;

    juce::AudioProcessorValueTreeState::SliderAttachment ratioAttach_, indexAttach_,
                                                         feedbackAttach_, subAttach_;

    void setupSlider (juce::Slider& s, juce::Label& nameLabel, const juce::String& name,
                      juce::Label& valueLabel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwoOpFMAudioProcessorEditor)
};
