#include "PluginEditor.h"


//==============================================================================
// Geometry
//==============================================================================
namespace {
    // ── FM slider columns: Ratio, Index, Feedback, Sub ─────────────────────
    // Evenly distributed; Sub aligns with Output knob (col 5).
    constexpr int kFMColX[]   = { 83, 209, 336, 463 };

    // ── Knob columns: Attack, Decay, Color, Output ─────────────────────────
    constexpr int kKnobColX[] = { 83, 210, 337, 463 };

    // ── FM slider geometry ─────────────────────────────────────────────────
    constexpr int kTrackTop    = 95;
    constexpr int kTrackBot    = 255;
    constexpr int kTrackHeight = kTrackBot - kTrackTop;   // 160
    constexpr int kTrackWidth  = 5;

    constexpr int kThumbW      = 42;
    constexpr int kThumbH      = 13;
    constexpr int kTickCount   = 10;

    constexpr int kSliderW     = 44;
    constexpr int kSliderTop   = kTrackTop - kThumbH / 2;            // 89
    constexpr int kSliderH     = kTrackBot - kSliderTop + kThumbH / 2; // 172

    // FM label
    constexpr int kFMLabelY    = 271;
    constexpr int kFMLabelH    = 14;
    constexpr int kFMLabelW    = 80;

    // ── Knob geometry ──────────────────────────────────────────────────────
    constexpr int   kKnobR     = 26;
    constexpr int   kKnobCY    = 372;
    constexpr int   kKnobW     = kKnobR * 2 + 24;  // 76 — extra margin for tick marks

    // Rotary sweep: −135° to +135° from 12 o'clock (270° total).
    constexpr float kRotaryStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kRotaryEnd   = juce::MathConstants<float>::pi * 2.75f;

    // Knob label
    constexpr int kKnobLabelY  = 412;
    constexpr int kKnobLabelH  = 14;
    constexpr int kKnobLabelW  = 80;

    // ── LPG section header ─────────────────────────────────────────────────
    constexpr int kLPGHeaderY  = 304;
    constexpr int kLPGHeaderH  = 14;

    // ── Ping pill ──────────────────────────────────────────────────────────
    constexpr int kPillW       = 38;
    constexpr int kPillH       = 11;
    // bottomLeft label text visual center sits ~9px into the 14px header rect;
    // offset pill so its centre matches.
    constexpr int kPillYOffset = 9 - kPillH / 2;  // = 3
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

    // ── Tick marks (skip top and bottom endpoints) ───────────────────────────
    g.setColour (juce::Colour (0xff888888));
    for (int i = 1; i < kTickCount - 1; ++i)
    {
        const float ty = localTrackTop + (float) i * (trackH / (float) (kTickCount - 1));
        g.drawLine (cx - 14.0f, ty, cx - 4.5f, ty, 1.0f);
        g.drawLine (cx + 4.5f, ty, cx + 14.0f, ty, 1.0f);
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

    // ── Tick marks ───────────────────────────────────────────────────────────
    constexpr int kNumTicks = 7;
    g.setColour (juce::Colour (0xff888888));
    for (int i = 0; i < kNumTicks; ++i)
    {
        const float t     = (float) i / (float) (kNumTicks - 1);
        const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float sinA  = std::sin (angle);
        const float cosA  = std::cos (angle);
        g.drawLine (cx + sinA * (r + 4.0f),  cy - cosA * (r + 4.0f),
                    cx + sinA * (r + 10.0f), cy - cosA * (r + 10.0f),
                    1.0f);
    }

    // ── Body ─────────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // ── Indicator line ───────────────────────────────────────────────────────
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float sinA  = std::sin (angle);
    const float cosA  = std::cos (angle);

    juce::Path indicator;
    indicator.startNewSubPath (cx + sinA * 9.75f,  cy - cosA * 9.75f);
    indicator.lineTo           (cx + sinA * 23.4f,  cy - cosA * 23.4f);

    g.setColour (juce::Colours::white);
    g.strokePath (indicator,
                  juce::PathStrokeType (3.3f,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
}

//==============================================================================
// PillLookAndFeel
//==============================================================================

void PillLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                        bool, bool)
{
    const float w      = (float) b.getWidth();
    const float h      = (float) b.getHeight();
    const float corner = h * 0.42f;

    if (b.getToggleState())
    {
        // PING: filled matte-black pill
        g.setColour (juce::Colour (0xff111111));
        g.fillRoundedRectangle (0.0f, 0.0f, w, h, corner);
        g.setColour (juce::Colours::white);
    }
    else
    {
        // GATE: outlined pill on panel background
        g.setColour (juce::Colour (0xff888888));
        g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, corner, 1.0f);
        g.setColour (juce::Colour (0xff444444));
    }

    g.setFont (juce::Font (juce::FontOptions().withHeight (7.5f).withStyle ("Bold")));
    g.drawText ("PING",
                0, 0, (int) w, (int) h, juce::Justification::centred);
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
      colorAttach_   (p.apvts, "color",    colorSlider_),
      outputAttach_  (p.apvts, "output",   outputSlider_),
      pingAttach_    (p.apvts, "ping",     pingButton_)
{
    setSize (545, 455);

    setupSlider (ratioSlider_,    ratioLabel_,    "Ratio");
    setupSlider (indexSlider_,    indexLabel_,    "Index");
    setupSlider (feedbackSlider_, feedbackLabel_, "Feedback");
    setupSlider (subSlider_,      subLabel_,      "Sub");

    setupKnob (attackSlider_, attackLabel_, "Attack");
    setupKnob (decaySlider_,  decayLabel_,  "Decay");
    setupKnob (colorSlider_, colorLabel_, "Color");
    setupKnob (outputSlider_, outputLabel_, "Output");

    pingButton_.setClickingTogglesState (true);
    pingButton_.setLookAndFeel (&pillLAF_);
    addAndMakeVisible (pingButton_);
}

TwoOpFMAudioProcessorEditor::~TwoOpFMAudioProcessorEditor()
{
    juce::Slider* all[] = { &ratioSlider_, &indexSlider_, &feedbackSlider_, &subSlider_,
                             &attackSlider_, &decaySlider_, &colorSlider_, &outputSlider_ };
    for (auto* s : all)
        s->setLookAndFeel (nullptr);
    pingButton_.setLookAndFeel (nullptr);
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
    // ── Panel background ─────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xffd8d8d8));
    g.fillAll();

    juce::ColourGradient overlay (juce::Colour (0x18ffffff), 0.0f, 0.0f,
                                  juce::Colour (0x18000000), 0.0f, (float) getHeight(), false);
    g.setGradientFill (overlay);
    g.fillAll();

    // ── Title strip ──────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff111111));
    g.setFont (juce::Font (juce::FontOptions().withHeight (30.0f).withStyle ("Bold")));
    g.drawText ("2-OP", 22, 9, 150, 37, juce::Justification::bottomLeft);

    g.setColour (juce::Colour (0xff666666));
    g.setFont (juce::Font (juce::FontOptions().withHeight (7.5f).withStyle ("Bold")));
    g.drawText ("FM SYNTHESIZER", 0, 14, getWidth() - 23, 14, juce::Justification::bottomRight);

    g.setColour (juce::Colour (0xff777777));
    g.setFont (juce::Font (juce::FontOptions().withHeight (7.5f)));
    g.drawText ("OPERATOR MODEL: 2-OP / FEEDBACK", 0, 28, getWidth() - 23, 14, juce::Justification::bottomRight);

    // ── FM OPERATORS section box ─────────────────────────────────────────────
    const juce::Rectangle<float> fmBox (14.0f, 60.0f, 517.0f, 228.0f);
    g.setColour (juce::Colour (0xffd0d0d0));
    g.fillRoundedRectangle (fmBox, 8.0f);
    g.setColour (juce::Colour (0xff888888));
    g.drawRoundedRectangle (fmBox, 8.0f, 1.5f);

    g.setColour (juce::Colour (0xff555555));
    g.setFont (juce::Font (juce::FontOptions().withHeight (8.0f).withStyle ("Bold")));
    g.drawText ("FM CONTROLS", 28, 68, 200, 14, juce::Justification::bottomLeft);

    // ── ENVELOPE + OUTPUT section box ────────────────────────────────────────
    const juce::Rectangle<float> envBox (14.0f, 296.0f, 517.0f, 149.0f);
    g.setColour (juce::Colour (0xffd0d0d0));
    g.fillRoundedRectangle (envBox, 8.0f);
    g.setColour (juce::Colour (0xff888888));
    g.drawRoundedRectangle (envBox, 8.0f, 1.5f);

    g.setColour (juce::Colour (0xff555555));
    g.setFont (juce::Font (juce::FontOptions().withHeight (8.0f).withStyle ("Bold")));
    g.drawText ("LPG + OUTPUT", 28, kLPGHeaderY, 250, kLPGHeaderH, juce::Justification::bottomLeft);

}

void TwoOpFMAudioProcessorEditor::resized()
{
    static_assert (std::size (kFMColX)   == 4, "");
    static_assert (std::size (kKnobColX) == 4, "");

    juce::Slider* fmSliders[] = { &ratioSlider_, &indexSlider_, &feedbackSlider_, &subSlider_ };
    juce::Label*  fmNames[]   = { &ratioLabel_,  &indexLabel_,  &feedbackLabel_,  &subLabel_ };

    for (int i = 0; i < 4; ++i)
    {
        const int cx = kFMColX[i];
        fmSliders[i]->setBounds (cx - kSliderW / 2, kSliderTop, kSliderW, kSliderH);
        fmNames  [i]->setBounds (cx - kFMLabelW / 2, kFMLabelY, kFMLabelW, kFMLabelH);
    }

    juce::Slider* knobs[]     = { &attackSlider_, &decaySlider_, &colorSlider_, &outputSlider_ };
    juce::Label*  knobNames[] = { &attackLabel_,  &decayLabel_,  &colorLabel_,  &outputLabel_ };

    for (int i = 0; i < 4; ++i)
    {
        const int cx = kKnobColX[i];
        knobs    [i]->setBounds (cx - kKnobW / 2, kKnobCY - kKnobW / 2, kKnobW, kKnobW);
        knobNames[i]->setBounds (cx - kKnobLabelW / 2, kKnobLabelY, kKnobLabelW, kKnobLabelH);
    }

    // Pill toggle: centered on Decay knob column, vertically centered in label row.
    pingButton_.setBounds (kKnobColX[1] - kPillW / 2, kLPGHeaderY + kPillYOffset, kPillW, kPillH);
}
