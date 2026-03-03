#include "PluginEditor.h"


//==============================================================================
// Geometry
//==============================================================================
namespace {
    // 4 columns across 400 px — 100 px apart, 50 px margins each side.
    // Centres at 50, 150, 250, 350.
    static const int kColX[] = { 50, 150, 250, 350 };

    // ── FM slider geometry ─────────────────────────────────────────────────
    static const int kTrackTop    = 44;
    static const int kTrackBot    = 168;
    static const int kTrackHeight = kTrackBot - kTrackTop;  // 124
    static const int kTrackWidth  = 4;

    static const int kThumbW      = 28;
    static const int kThumbH      = 10;
    static const int kTickCount   = 10;

    static const int kSliderW     = 44;
    // kSliderTop = kTrackTop - thumbRadius so that sliderPos at max equals
    // localTrackTop, keeping the thumb fully inside the component at both extremes.
    static const int kSliderTop   = kTrackTop - kThumbH / 2;       // 39
    static const int kSliderH     = kTrackBot - kSliderTop + kThumbH / 2; // 134

    // FM label position
    static const int kFMLabelY    = 176;
    static const int kFMLabelH    = 14;

    // ── Section separator ──────────────────────────────────────────────────
    static const int kSeparatorY  = 210;

    // ── ADSR knob geometry ─────────────────────────────────────────────────
    static const int   kKnobR     = 16;
    static const int   kKnobCY    = 246;  // panel-coord centre-y of knob body
    static const int   kKnobW     = kKnobR * 2 + 4;  // component size = 36

    // Rotary sweep: −135° to +135° from 12 o'clock (270° total).
    static const float kRotaryStart = juce::MathConstants<float>::pi * 1.25f;
    static const float kRotaryEnd   = juce::MathConstants<float>::pi * 2.75f;

    // ADSR label position
    static const int kADSRLabelY  = 270;
    static const int kADSRLabelH  = 14;
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
    g.fillRect (cx - kTrackWidth * 0.5f, localTrackTop, (float) kTrackWidth, trackH);

    // ── Thumb ────────────────────────────────────────────────────────────────
    const float thumbCy = sliderPos;
    const float thumbX  = cx - kThumbW * 0.5f;
    const float thumbY  = thumbCy - kThumbH * 0.5f;

    g.setColour (juce::Colour (0xff111111));
    g.fillRect (thumbX, thumbY, (float) kThumbW, (float) kThumbH);

    g.setColour (juce::Colours::white);
    g.drawLine (thumbX, thumbCy, thumbX + kThumbW, thumbCy, 1.0f);
}

//==============================================================================
// ADSRKnobLookAndFeel
//==============================================================================

void ADSRKnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPos,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider&)
{
    const float cx = x + width  * 0.5f;
    const float cy = y + height * 0.5f;
    const float r  = (float) kKnobR;

    // ── Body ─────────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // ── Indicator line ───────────────────────────────────────────────────────
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float sinA  = std::sin (angle);
    const float cosA  = std::cos (angle);

    juce::Path indicator;
    indicator.startNewSubPath (cx + sinA * 5.0f,  cy - cosA * 5.0f);
    indicator.lineTo           (cx + sinA * 12.0f, cy - cosA * 12.0f);

    g.setColour (juce::Colours::white);
    g.strokePath (indicator,
                  juce::PathStrokeType (2.2f,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
}

//==============================================================================
// TwoOpFMAudioProcessorEditor
//==============================================================================

TwoOpFMAudioProcessorEditor::TwoOpFMAudioProcessorEditor (TwoOpFMAudioProcessor& p)
    : AudioProcessorEditor (&p),
      ratioAttach_   (p.apvts, "ratio",    ratioSlider_),
      indexAttach_   (p.apvts, "index",    indexSlider_),
      feedbackAttach_(p.apvts, "feedback", feedbackSlider_),
      subAttach_     (p.apvts, "sub",      subSlider_),
      attackAttach_  (p.apvts, "attack",   attackSlider_),
      decayAttach_   (p.apvts, "decay",    decaySlider_),
      sustainAttach_ (p.apvts, "sustain",  sustainSlider_),
      releaseAttach_ (p.apvts, "release",  releaseSlider_)
{
    setSize (400, 300);

    setupSlider (ratioSlider_,    ratioLabel_,    "RATIO");
    setupSlider (indexSlider_,    indexLabel_,    "INDEX");
    setupSlider (feedbackSlider_, feedbackLabel_, "FDBK");
    setupSlider (subSlider_,      subLabel_,      "SUB");

    setupKnob (attackSlider_,  attackLabel_,  "ATK");
    setupKnob (decaySlider_,   decayLabel_,   "DCY");
    setupKnob (sustainSlider_, sustainLabel_, "SUS");
    setupKnob (releaseSlider_, releaseLabel_, "REL");
}

TwoOpFMAudioProcessorEditor::~TwoOpFMAudioProcessorEditor()
{
    juce::Slider* all[] = { &ratioSlider_, &indexSlider_, &feedbackSlider_, &subSlider_,
                             &attackSlider_, &decaySlider_, &sustainSlider_, &releaseSlider_ };
    for (auto* s : all)
        s->setLookAndFeel (nullptr);
}

void TwoOpFMAudioProcessorEditor::setupSlider (juce::Slider& s, juce::Label& nameLabel,
                                                const juce::String& name)
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
}

void TwoOpFMAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& nameLabel,
                                              const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (kRotaryStart, kRotaryEnd, true);
    s.setLookAndFeel (&knobLAF_);
    addAndMakeVisible (s);

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (juce::Font (juce::FontOptions().withHeight (10.0f).withStyle ("Bold")));
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff444444));
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);
}

void TwoOpFMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xffd8d8d8));
    g.fillAll();

    juce::ColourGradient overlay (juce::Colour (0x18ffffff), 0.0f, 0.0f,
                                  juce::Colour (0x18000000), 0.0f, (float) getHeight(), false);
    g.setGradientFill (overlay);
    g.fillAll();

    // Plugin title
    g.setColour (juce::Colour (0xff222222));
    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    g.drawText ("2-OP", 0, 0, getWidth(), 44, juce::Justification::centred);

    // FM / ADSR section separator
    g.setColour (juce::Colour (0x40000000));
    g.drawLine (20.0f, (float) kSeparatorY,
                (float) (getWidth() - 20), (float) kSeparatorY, 0.75f);
}

void TwoOpFMAudioProcessorEditor::resized()
{
    static_assert (std::size (kColX) == 4, "");

    juce::Slider* fmSliders[] = { &ratioSlider_, &indexSlider_, &feedbackSlider_, &subSlider_ };
    juce::Label*  fmNames[]   = { &ratioLabel_,  &indexLabel_,  &feedbackLabel_,  &subLabel_ };

    juce::Slider* envKnobs[]  = { &attackSlider_, &decaySlider_, &sustainSlider_, &releaseSlider_ };
    juce::Label*  envNames[]  = { &attackLabel_,  &decayLabel_,  &sustainLabel_,  &releaseLabel_ };

    for (int i = 0; i < 4; ++i)
    {
        const int cx = kColX[i];

        // FM sliders
        fmSliders[i]->setBounds (cx - kSliderW / 2, kSliderTop, kSliderW, kSliderH);
        fmNames  [i]->setBounds (cx - 25, kFMLabelY, 50, kFMLabelH);

        // ADSR knobs
        envKnobs [i]->setBounds (cx - kKnobW / 2, kKnobCY - kKnobR - 2, kKnobW, kKnobW);
        envNames [i]->setBounds (cx - 25, kADSRLabelY, 50, kADSRLabelH);
    }
}
