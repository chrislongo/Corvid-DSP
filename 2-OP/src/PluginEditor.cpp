#include "PluginEditor.h"

//==============================================================================
// Geometry constants (pixel-exact from SVG mockup)
//==============================================================================
namespace {
    // Column centres for the 4 sliders
    static const int kColX[]  = { 55, 118, 182, 245 };

    static const int kTrackTop    = 52;
    static const int kTrackBot    = 212;
    static const int kTrackHeight = kTrackBot - kTrackTop;  // 160
    static const int kTrackWidth  = 4;

    static const int kThumbW      = 28;
    static const int kThumbH      = 10;

    static const int kTickCount   = 10;   // ticks between endpoints

    static const int kLabelY      = 220;
    static const int kLabelH      = 14;
    static const int kValueY      = 236;
    static const int kValueH      = 14;

    // Slider component bounds (wide enough for comfortable clicking)
    static const int kSliderW     = 40;
    static const int kSliderTop   = 42;
    static const int kSliderH     = kTrackBot - kSliderTop + kThumbH / 2 + 2;
}

//==============================================================================
// FMSliderLookAndFeel
//==============================================================================

FMSliderLookAndFeel::FMSliderLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xff333333));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
}

void FMSliderLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                             int /*x*/, int /*y*/, int /*width*/, int /*height*/,
                                             float sliderPos,
                                             float /*minSliderPos*/,
                                             float /*maxSliderPos*/,
                                             juce::Slider::SliderStyle,
                                             juce::Slider& slider)
{
    // All drawing is in slider-local coordinates.
    // The slider component spans from kSliderTop to kTrackBot (in parent space),
    // so in local space: track top = kTrackTop - kSliderTop, track bottom = kTrackBot - kSliderTop.
    const float localTrackTop = (float) (kTrackTop - kSliderTop);
    const float localTrackBot = (float) (kTrackBot - kSliderTop);
    const float trackH        = localTrackBot - localTrackTop;
    const float cx            = slider.getWidth() * 0.5f;

    // ── Tick marks ───────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff888888));
    for (int i = 1; i <= kTickCount; ++i)
    {
        const float ty = localTrackTop + (float) i * (trackH / (kTickCount + 1));
        g.drawLine (cx - 9.0f, ty, cx - 3.0f, ty, 1.0f);
        g.drawLine (cx + 3.0f, ty, cx + 9.0f, ty, 1.0f);
    }

    // ── Track ────────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRect (cx - kTrackWidth * 0.5f, localTrackTop,
                (float) kTrackWidth, trackH);

    // ── Thumb ────────────────────────────────────────────────────────────────
    // sliderPos is the y-coordinate of the thumb centre in local slider space.
    const float thumbCy = sliderPos;
    const float thumbX  = cx - kThumbW * 0.5f;
    const float thumbY  = thumbCy - kThumbH * 0.5f;

    g.setColour (juce::Colour (0xff111111));
    g.fillRect (thumbX, thumbY, (float) kThumbW, (float) kThumbH);

    // White hairline centred on thumb face
    g.setColour (juce::Colours::white);
    g.drawLine (thumbX, thumbCy, thumbX + kThumbW, thumbCy, 1.0f);
}

//==============================================================================
// TwoOpFMAudioProcessorEditor
//==============================================================================

TwoOpFMAudioProcessorEditor::TwoOpFMAudioProcessorEditor (TwoOpFMAudioProcessor& p)
    : AudioProcessorEditor (&p),
      ratioAttach_    (p.apvts, "ratio",    ratioSlider_),
      indexAttach_    (p.apvts, "index",    indexSlider_),
      feedbackAttach_ (p.apvts, "feedback", feedbackSlider_),
      subAttach_      (p.apvts, "sub",      subSlider_)
{
    setSize (300, 280);

    setupSlider (ratioSlider_,    ratioLabel_,    "RATIO",    ratioValue_);
    setupSlider (indexSlider_,    indexLabel_,    "INDEX",    indexValue_);
    setupSlider (feedbackSlider_, feedbackLabel_, "FDBK",     feedbackValue_);
    setupSlider (subSlider_,      subLabel_,      "SUB",      subValue_);

    // Wire value labels to sliders
    auto wire = [](juce::Slider& s, juce::Label& lbl) {
        s.onValueChange = [&s, &lbl] {
            lbl.setText (juce::String (s.getValue(), 2), juce::dontSendNotification);
        };
        // Set initial text
        lbl.setText (juce::String (s.getValue(), 2), juce::dontSendNotification);
    };
    wire (ratioSlider_,    ratioValue_);
    wire (indexSlider_,    indexValue_);
    wire (feedbackSlider_, feedbackValue_);
    wire (subSlider_,      subValue_);
}

TwoOpFMAudioProcessorEditor::~TwoOpFMAudioProcessorEditor()
{
    ratioSlider_   .setLookAndFeel (nullptr);
    indexSlider_   .setLookAndFeel (nullptr);
    feedbackSlider_.setLookAndFeel (nullptr);
    subSlider_     .setLookAndFeel (nullptr);
}

void TwoOpFMAudioProcessorEditor::setupSlider (juce::Slider& s, juce::Label& nameLabel,
                                                const juce::String& name, juce::Label& valueLabel)
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&sliderLAF_);
    addAndMakeVisible (s);

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (juce::Font (juce::FontOptions().withHeight (10.0f).withStyle ("Bold")));
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff444444));
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);

    valueLabel.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    valueLabel.setColour (juce::Label::textColourId, juce::Colour (0xff555555));
    valueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (valueLabel);
}

void TwoOpFMAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Silver-grey base
    g.setColour (juce::Colour (0xffd8d8d8));
    g.fillAll();

    // Subtle top-to-bottom gradient overlay
    juce::ColourGradient overlay (juce::Colour (0x18ffffff), 0.0f, 0.0f,
                                  juce::Colour (0x18000000), 0.0f, (float) getHeight(), false);
    g.setGradientFill (overlay);
    g.fillAll();

    // Plugin title
    g.setColour (juce::Colour (0xff222222));
    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    g.drawText ("2-OP", 0, 0, getWidth(), 44, juce::Justification::centred);
}

void TwoOpFMAudioProcessorEditor::resized()
{
    juce::Slider* sliders[] = { &ratioSlider_, &indexSlider_, &feedbackSlider_, &subSlider_ };
    juce::Label*  names[]  = { &ratioLabel_,  &indexLabel_,  &feedbackLabel_,  &subLabel_  };
    juce::Label*  values[] = { &ratioValue_,  &indexValue_,  &feedbackValue_,  &subValue_  };

    static_assert (std::size (kColX) == 4, "");

    for (int i = 0; i < 4; ++i)
    {
        const int cx = kColX[i];
        sliders[i]->setBounds (cx - kSliderW / 2, kSliderTop, kSliderW, kSliderH);
        names  [i]->setBounds (cx - 24, kLabelY, 48, kLabelH);
        values [i]->setBounds (cx - 24, kValueY, 48, kValueH);
    }
}
