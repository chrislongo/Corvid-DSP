#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// FMSliderLookAndFeel
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

    int getSliderThumbRadius (juce::Slider&) override { return 5; }  // kThumbH / 2
};

//==============================================================================
// ADSRKnobLookAndFeel
// Matte-black circle (r=16) with a white indicator line (r=5 to r=12, 2.2 px,
// round caps). 270° sweep matching the FM slider aesthetic.
class ADSRKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
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
    ADSRKnobLookAndFeel knobLAF_;

    // FM sliders (top section)
    juce::Slider ratioSlider_, indexSlider_, feedbackSlider_, subSlider_;
    juce::Label  ratioLabel_,  indexLabel_,  feedbackLabel_,  subLabel_;

    // ADSR knobs (bottom section)
    juce::Slider attackSlider_, decaySlider_, sustainSlider_, releaseSlider_;
    juce::Label  attackLabel_,  decayLabel_,  sustainLabel_,  releaseLabel_;

    juce::AudioProcessorValueTreeState::SliderAttachment
        ratioAttach_, indexAttach_, feedbackAttach_, subAttach_,
        attackAttach_, decayAttach_, sustainAttach_, releaseAttach_;

    void setupSlider (juce::Slider& s, juce::Label& nameLabel, const juce::String& name);
    void setupKnob   (juce::Slider& s, juce::Label& nameLabel, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwoOpFMAudioProcessorEditor)
};
