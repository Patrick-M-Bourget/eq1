#include "AnalyzerPopover.h"

#include "Accessibility.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Icons.h"
#include "staple/Light.h"
#include "staple/LookAndFeel.h"

#include <array>
#include <cmath>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr int cardWidth = 260, padding = 10, gap = 10;
constexpr int sourceHeight = 28, sourcePadding = 2, sourceGap = 4, rowHeight = 28, rowPadding = 8, peakHoldHeight = 30;
constexpr int numRows = 4;
constexpr int cardHeight = padding + sourceHeight + 2 * sourcePadding + gap + numRows * rowHeight + gap + 1 + gap + peakHoldHeight + padding;
constexpr float dotSize = 5.0f, dotGap = 7.0f;
constexpr int buttonMinimumWidth = 92, buttonPaddingLeft = 12, buttonPaddingRight = 10, chevronSize = 10, chevronGap = 10;
constexpr float chevronAlpha = 0.6f;

// Analyzer Tilt's listed values, which a click cycles through; the arrow keys step it by tiltStep.
constexpr std::array<double, 4> listedTilts { 0.0, 3.0, 4.5, 6.0 };
constexpr double tiltStep = 0.5, maximumTilt = 6.0;
constexpr int numTilts = static_cast<int> (maximumTilt / tiltStep) + 1;

juce::Font labelFont() { return staple::font (size::fs3, staple::Weight::medium); }

juce::String tiltText (double tilt)
{
    if (tilt <= 0.0)
        return "Off";
    const bool whole = std::abs (tilt - std::round (tilt)) < 1.0e-6;
    return (whole ? juce::String (juce::roundToInt (tilt)) : juce::String (tilt, 1)) + " dB/oct";
}
} // namespace

juce::String analyzerButtonText (const AnalyzerSettings& settings)
{
    if (settings.showPreEq && settings.showPostEq)
        return "Pre + Post";
    if (settings.showPreEq)
        return "Pre";
    return settings.showPostEq ? "Post" : "Off";
}

double nextAnalyzerTilt (double tilt)
{
    for (double listed : listedTilts)
        if (listed > tilt + 1.0e-6)
            return listed;
    return listedTilts.front();
}

AnalyzerButton::AnalyzerButton() : staple::TextChip (analyzerButtonText ({}), Look::filled)
{
    setName ("Analyzer");
    setTitle ("Analyzer");
    setTooltip ("Analyzer: what it shows and how. Click for its settings");
}

int AnalyzerButton::getIdealWidth() const
{
    const int text = staple::textWidth (labelFont(), getButtonText());
    return std::max (buttonMinimumWidth, buttonPaddingLeft + text + chevronGap + chevronSize + buttonPaddingRight);
}

void AnalyzerButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = staple::enabledAlpha (*this);
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (staple::lit (colour::fill1, highlighted, down).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (bounds, size::r2);

    auto area = getLocalBounds().withTrimmedLeft (buttonPaddingLeft).withTrimmedRight (buttonPaddingRight).toFloat();
    const auto ink = colour::text1.withMultipliedAlpha (alpha);
    staple::drawIcon (g, staple::Icon::chevronUp, area.removeFromRight (static_cast<float> (chevronSize)).withSizeKeepingCentre (chevronSize, chevronSize),
                      ink.withMultipliedAlpha (chevronAlpha));
    area.removeFromRight (static_cast<float> (chevronGap));
    g.setFont (labelFont());
    g.setColour (ink);
    g.drawText (getButtonText(), area, juce::Justification::centredLeft, true);
}

std::unique_ptr<juce::AccessibilityHandler> AnalyzerButton::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return getButtonText(); }, [this] { triggerClick(); });
}

AnalyzerRow::AnalyzerRow (const juce::String& title, const juce::String& l) : juce::ComboBox (title), label (l)
{
    setTitle (title);
    // The row draws its value itself.
    setColour (juce::ComboBox::textColourId, colour::text1.withAlpha (0.0f));
    setHasFocusOutline (true);
    setMouseClickGrabsKeyboardFocus (false);
    setRepaintsOnMouseActivity (true);
}

int AnalyzerRow::nextIndex() const
{
    const int n = getNumItems();
    return n > 0 ? (getSelectedItemIndex() + n - 1) % n : -1;
}

void AnalyzerRow::cycle()
{
    if (isEnabled() && getNumItems() > 0)
        setSelectedItemIndex (nextIndex(), juce::sendNotificationSync);
}

void AnalyzerRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown() && ! e.mods.isPopupMenu())
        cycle();
}

bool AnalyzerRow::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if ((code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey) && ! key.getModifiers().isAnyModifierKeyDown())
    {
        staple::LookAndFeel::keyUsed (*this);
        cycle();
        return true;
    }
    return juce::ComboBox::keyPressed (key);
}

void AnalyzerRow::paint (juce::Graphics& g)
{
    const float alpha = staple::enabledAlpha (*this);
    if (isMouseOver (true))
    {
        g.setColour (colour::fill1.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), size::r2);
    }
    const auto area = getLocalBounds().reduced (rowPadding, 0);
    g.setFont (staple::font (size::fs3));
    g.setColour (colour::text3.withMultipliedAlpha (alpha));
    g.drawText (label, area, juce::Justification::centredLeft, true);
    g.setColour (colour::text1.withMultipliedAlpha (alpha));
    g.drawText (getText(), area, juce::Justification::centredRight, true);
}

// Analyzer Tilt's row: its items run from 6 dB/oct down to Off in 0.5 dB/oct steps, so the up key raises
// it, as on every row, and a click goes to the next listed value above.
class AnalyzerPopover::TiltRow final : public AnalyzerRow
{
public:
    TiltRow() : AnalyzerRow ("Analyzer Tilt", "Analyzer Tilt")
    {
        for (int i = 0; i < numTilts; ++i)
            addItem (tiltText (tiltOf (i)), i + 1);
    }

    double tilt() const { return tiltOf (getSelectedItemIndex()); }
    void show (double tilt) { setSelectedItemIndex (juce::roundToInt ((maximumTilt - juce::jlimit (0.0, maximumTilt, tilt)) / tiltStep), juce::dontSendNotification); }

protected:
    int nextIndex() const override { return juce::roundToInt ((maximumTilt - nextAnalyzerTilt (tilt())) / tiltStep); }

private:
    static double tiltOf (int index) { return maximumTilt - tiltStep * index; }
};

// One of Pre, Post and Sidechain: its dot and name, on fill2 in text1 while on, else in text3 with the
// dot in text4.
class AnalyzerPopover::SourceButton final : public juce::Button
{
public:
    SourceButton (const juce::String& text, const juce::String& title) : juce::Button (title)
    {
        setButtonText (text);
        setTitle (title);
        setClickingTogglesState (true);
        setHasFocusOutline (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const bool on = getToggleState();
        const auto bounds = getLocalBounds().toFloat();
        if (on)
        {
            g.setColour (staple::lit (colour::fill2, highlighted, down));
            g.fillRoundedRectangle (bounds, size::r2);
        }
        const auto font = labelFont();
        const float width = dotSize + dotGap + juce::GlyphArrangement::getStringWidth (font, getButtonText());
        auto row = bounds.withSizeKeepingCentre (std::min (width, bounds.getWidth()), bounds.getHeight());
        g.setColour (on ? colour::text1 : colour::text4);
        g.fillEllipse (row.removeFromLeft (dotSize).withSizeKeepingCentre (dotSize, dotSize));
        row.removeFromLeft (dotGap);
        g.setFont (font);
        g.setColour (on || highlighted || down ? colour::text1 : colour::text3);
        g.drawText (getButtonText(), row, juce::Justification::centredLeft, false);
    }
};

// Peak Hold's toggle: its icon and "Peak Hold", on fill2 in text1 while on, else in text3.
class AnalyzerPopover::PeakHoldButton final : public juce::Button
{
public:
    PeakHoldButton() : juce::Button ("Peak Hold")
    {
        setButtonText ("Peak Hold");
        setTitle ("Analyzer Peak Hold");
        setTooltip ("Peak Hold: the slower outline above the live spectrum");
        setClickingTogglesState (true);
        setHasFocusOutline (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const bool on = getToggleState();
        const auto bounds = getLocalBounds().toFloat();
        if (on)
        {
            g.setColour (staple::lit (colour::fill2, highlighted, down));
            g.fillRoundedRectangle (bounds, size::r2);
        }
        const auto ink = on || highlighted || down ? colour::text1 : colour::text3;
        const auto font = labelFont();
        constexpr float iconWidth = 16.0f, iconHeight = 12.0f;
        const float width = iconWidth + dotGap + juce::GlyphArrangement::getStringWidth (font, getButtonText());
        auto row = bounds.withSizeKeepingCentre (std::min (width, bounds.getWidth()), bounds.getHeight());
        staple::drawIcon (g, staple::Icon::peakHold, row.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, iconHeight), ink);
        row.removeFromLeft (dotGap);
        g.setFont (font);
        g.setColour (ink);
        g.drawText (getButtonText(), row, juce::Justification::centredLeft, false);
    }
};

AnalyzerPopover::AnalyzerPopover (PluginProcessor& p)
    : processor (p), showPreEq (std::make_unique<SourceButton> ("Pre", "Analyzer Pre-EQ")),
      showPostEq (std::make_unique<SourceButton> ("Post", "Analyzer Post-EQ")),
      showSidechain (std::make_unique<SourceButton> ("Sidechain", "Analyzer Sidechain")), range ("Analyzer Range", "Range"),
      resolution ("Analyzer Resolution", "Resolution"), speed ("Analyzer Speed", "Speed"), tilt (std::make_unique<TiltRow>()),
      peakHold (std::make_unique<PeakHoldButton>())
{
    showPreEq->setTooltip ("Show the spectrum before the EQ");
    showPostEq->setTooltip ("Show the spectrum after the EQ");
    showSidechain->setTooltip ("Show the Sidechain spectrum");
    // Largest value first, so the up key steps to the larger one (KeyboardControl steps up to the
    // previous item); the ids are the settings' values.
    for (int db : { 120, 90, 60 })
        range.addItem (juce::String (db) + " dB", db);
    const juce::StringArray resolutions { "Low", "Medium", "High", "Maximum" }, speeds { "Very Slow", "Slow", "Medium", "Fast", "Very Fast" };
    for (int i = resolutions.size(); --i >= 0;)
        resolution.addItem (resolutions[i], i + 1);
    for (int i = speeds.size(); --i >= 0;)
        speed.addItem (speeds[i], i + 1);

    int order = 0;
    for (juce::Button* button : { showPreEq.get(), showPostEq.get(), showSidechain.get() })
    {
        button->onClick = [this] { store(); };
        button->setExplicitFocusOrder (++order);
        addAndMakeVisible (*button);
    }
    for (AnalyzerRow* row : { &range, &resolution, &speed, static_cast<AnalyzerRow*> (tilt.get()) })
    {
        row->onChange = [this] { store(); };
        row->setExplicitFocusOrder (++order);
        addAndMakeVisible (*row);
    }
    peakHold->onClick = [this] { store(); };
    peakHold->setExplicitFocusOrder (++order);
    addAndMakeVisible (*peakHold);

    setCardSize (cardWidth, cardHeight);
    show();
}

AnalyzerPopover::~AnalyzerPopover() { follow.stopTimer(); }

void AnalyzerPopover::openFrom (juce::Component& button, juce::Component& alignTo)
{
    show();
    open (button, Placement::above);
    if (auto* layer = getParentComponent(); layer != nullptr && isOpen())
    {
        const int left = layer->getLocalArea (&alignTo, alignTo.getLocalBounds()).getX();
        setTopLeftPosition (left - getCardBounds().getX(), getY());
        follow.startTimerHz (4);
    }
}

void AnalyzerPopover::show()
{
    if (! isOpen())
        follow.stopTimer();
    const auto settings = processor.analyzerSettings();
    showPreEq->setToggleState (settings.showPreEq, juce::dontSendNotification);
    showPostEq->setToggleState (settings.showPostEq, juce::dontSendNotification);
    showSidechain->setToggleState (settings.showSidechain, juce::dontSendNotification);
    peakHold->setToggleState (settings.peakHold, juce::dontSendNotification);
    range.setSelectedId (settings.rangeDb, juce::dontSendNotification);
    resolution.setSelectedId (static_cast<int> (settings.resolution) + 1, juce::dontSendNotification);
    speed.setSelectedId (static_cast<int> (settings.speed) + 1, juce::dontSendNotification);
    tilt->show (settings.tiltDbPerOctave);
}

void AnalyzerPopover::store()
{
    processor.setAnalyzerSettings ({ .showPreEq = showPreEq->getToggleState(),
                                     .showPostEq = showPostEq->getToggleState(),
                                     .showSidechain = showSidechain->getToggleState(),
                                     .rangeDb = range.getSelectedId(),
                                     .speed = static_cast<AnalyzerSpeed> (speed.getSelectedId() - 1),
                                     .resolution = static_cast<AnalyzerResolution> (resolution.getSelectedId() - 1),
                                     .tiltDbPerOctave = tilt->tilt(),
                                     .peakHold = peakHold->getToggleState() });
    if (onSettingsChanged != nullptr)
        onSettingsChanged();
}

juce::Rectangle<int> AnalyzerPopover::sourcesArea() const
{
    return getCardBounds().reduced (padding).removeFromTop (sourceHeight + 2 * sourcePadding);
}

void AnalyzerPopover::paint (juce::Graphics& g)
{
    staple::Popover::paint (g);
    g.setColour (colour::surface1);
    g.fillRoundedRectangle (sourcesArea().toFloat(), size::r2);
    // The line above Peak Hold.
    g.setColour (colour::line2);
    g.fillRect (peakHold->getX(), peakHold->getY() - gap - 1, peakHold->getWidth(), 1);
}

void AnalyzerPopover::resized()
{
    auto sources = sourcesArea().reduced (sourcePadding);
    const int sourceWidth = (sources.getWidth() - 2 * sourceGap) / 3;
    for (auto* button : { showPreEq.get(), showPostEq.get(), showSidechain.get() })
    {
        button->setBounds (sources.removeFromLeft (sourceWidth));
        sources.removeFromLeft (sourceGap);
    }
    auto area = getCardBounds().reduced (padding).withTrimmedTop (sourceHeight + 2 * sourcePadding + gap);
    for (AnalyzerRow* row : { &range, &resolution, &speed, static_cast<AnalyzerRow*> (tilt.get()) })
        row->setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (gap + 1 + gap);
    peakHold->setBounds (area.removeFromTop (peakHoldHeight));
}

} // namespace eq1
