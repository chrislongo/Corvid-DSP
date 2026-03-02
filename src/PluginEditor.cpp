#include "PluginEditor.h"

//==============================================================================
// MetallicKnobLookAndFeel  (renamed conceptually: now a clean black knob)
//==============================================================================

MetallicKnobLookAndFeel::MetallicKnobLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xff1a1a1a));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  juce::Colour (0x40000000));
}

void MetallicKnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                int x, int y, int width, int height,
                                                float sliderPosProportional,
                                                float rotaryStartAngle,
                                                float rotaryEndAngle,
                                                juce::Slider&)
{
    const float cx = x + width  * 0.5f;
    const float cy = y + height * 0.5f;
    const float r  = juce::jmin (width, height) * 0.5f - 2.0f;

    // ── Drop shadow ──────────────────────────────────────────────────────────
    {
        juce::ColourGradient shadow (juce::Colour (0x50000000), cx, cy + r * 0.1f,
                                     juce::Colours::transparentBlack, cx, cy + r * 1.3f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (cx - r - 3.0f, cy - r + 4.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
    }

    // ── Black body ───────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // ── Subtle highlight on top edge ─────────────────────────────────────────
    {
        juce::ColourGradient highlight (juce::Colour (0x25ffffff), cx, cy - r,
                                        juce::Colours::transparentBlack, cx, cy, false);
        g.setGradientFill (highlight);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // ── White indicator line ─────────────────────────────────────────────────
    {
        const float angle = rotaryStartAngle
                          + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float sinA  = std::sin (angle);
        const float cosA  = -std::cos (angle);

        const float innerR = r * 0.30f;
        const float outerR = r * 0.78f;

        juce::Path line;
        line.startNewSubPath (cx + sinA * innerR, cy + cosA * innerR);
        line.lineTo          (cx + sinA * outerR, cy + cosA * outerR);

        g.setColour (juce::Colours::white);
        g.strokePath (line, juce::PathStrokeType (2.2f));
    }
}

juce::Label* MetallicKnobLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, juce::Colour (0xff1a1a1a));
    return label;
}

//==============================================================================
// OddHarmonicsAudioProcessorEditor
//==============================================================================

OddHarmonicsAudioProcessorEditor::OddHarmonicsAudioProcessorEditor (OddHarmonicsAudioProcessor& p)
    : AudioProcessorEditor (&p),
      harmonicsAttachment (p.apvts, "amount", harmonicsKnob)
{
    setSize (300, 220);

    // Knob
    harmonicsKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    harmonicsKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    harmonicsKnob.setLookAndFeel (&blackKnobLAF);
    addAndMakeVisible (harmonicsKnob);

    // Knob label — bold, above knob
    harmonicsLabel.setText ("HARMONICS", juce::dontSendNotification);
    harmonicsLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)
                                                           .withStyle ("Bold")));
    harmonicsLabel.setColour (juce::Label::textColourId, juce::Colour (0xff111111));
    harmonicsLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (harmonicsLabel);
}

OddHarmonicsAudioProcessorEditor::~OddHarmonicsAudioProcessorEditor()
{
    harmonicsKnob.setLookAndFeel (nullptr);
}

void OddHarmonicsAudioProcessorEditor::paint (juce::Graphics& g)
{
    // ── Light silver-grey background ──────────────────────────────────────────
    g.setColour (juce::Colour (0xffd8d8d8));
    g.fillAll();

    // Subtle top-to-bottom gradient overlay
    juce::ColourGradient bodyGrad (juce::Colour (0x18ffffff), 0.0f, 0.0f,
                                   juce::Colour (0x18000000), 0.0f, float (getHeight()), false);
    g.setGradientFill (bodyGrad);
    g.fillAll();
}

void OddHarmonicsAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Total height of knob group: label (15) + gap (4) + knob (100) + textbox (20) = 139
    const int knobSize    = 100;
    const int labelH      = 15;
    const int textBoxH    = 20;
    const int groupH      = labelH + 4 + knobSize + textBoxH;
    const int groupY      = (h - groupH) / 2;
    const int knobX       = (w - knobSize) / 2;

    harmonicsLabel.setBounds (knobX - 20, groupY, knobSize + 40, labelH);
    harmonicsKnob .setBounds (knobX, groupY + labelH + 4, knobSize, knobSize + textBoxH);
}
