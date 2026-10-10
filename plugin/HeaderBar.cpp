#include "HeaderBar.h"

#include "Accessibility.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Light.h"
#include "staple/controls/Overlay.h"

#include <cmath>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr int sidePadding = 6, groupGap = 16, buttonGap = 4, compareMargin = 4;
constexpr int iconButtonSize = 32, chipHeight = 32, chipPadding = 10;

juce::Font wordmarkFont() { return staple::font (size::fs5, staple::Weight::semiBold).withExtraKerningFactor (-0.01f); }
juce::Font letterFont() { return staple::font (size::fs4, staple::Weight::semiBold).withExtraKerningFactor (0.02f); }
juce::Font slashFont() { return staple::font (size::fs4, staple::Weight::regular).withExtraKerningFactor (0.02f); }

int widthOf (const juce::Font& font, const juce::String& text)
{
    return static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (font, text)));
}

CompareSide otherSide (CompareSide side) { return side == CompareSide::A ? CompareSide::B : CompareSide::A; }
juce::String letter (CompareSide side) { return side == CompareSide::A ? "A" : "B"; }
} // namespace

CompareButton::CompareButton() : staple::TextChip ({}, Look::plain, size::fs4)
{
    setName ("A/B Compare");
    setTitle ("A/B Compare");
}

void CompareButton::showSide (CompareSide newSide)
{
    if (newSide == side)
        return;
    previousSide = side;
    side = newSide;
    changedAt = juce::Time::getMillisecondCounterHiRes();
    setTooltip ("Switch to " + letter (otherSide (side)));
    startTimerHz (60);
    repaint();
}

float CompareButton::progress() const
{
    return staple::ease (juce::jlimit (0.0f, 1.0f, static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - changedAt) / staple::tokens::motion::dur2Ms)));
}

juce::Colour CompareButton::letterInk (CompareSide l) const
{
    const auto inkOn = [l] (CompareSide s) { return l == s ? colour::text1 : colour::text4; };
    return inkOn (previousSide).interpolatedWith (inkOn (side), progress());
}

void CompareButton::timerCallback()
{
    repaint();
    if (progress() >= 1.0f)
        stopTimer();
}

int CompareButton::getIdealWidth() const
{
    return widthOf (letterFont(), "AB") + widthOf (slashFont(), "/") + 2 + 2 * chipPadding;
}

void CompareButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    // The hover box, with no text of its own.
    staple::TextChip::paintButton (g, highlighted, down);
    const float alpha = staple::enabledAlpha (*this);
    const int a = widthOf (letterFont(), "A"), slash = widthOf (slashFont(), "/"), b = widthOf (letterFont(), "B");
    auto area = getLocalBounds().withSizeKeepingCentre (a + slash + b + 2, getHeight());
    g.setFont (letterFont());
    g.setColour (letterInk (CompareSide::A).withMultipliedAlpha (alpha));
    g.drawText ("A", area.removeFromLeft (a), juce::Justification::centred, false);
    area.removeFromLeft (1);
    g.setFont (slashFont());
    g.setColour (colour::text4.withMultipliedAlpha (alpha));
    g.drawText ("/", area.removeFromLeft (slash), juce::Justification::centred, false);
    area.removeFromLeft (1);
    g.setFont (letterFont());
    g.setColour (letterInk (CompareSide::B).withMultipliedAlpha (alpha));
    g.drawText ("B", area.removeFromLeft (b), juce::Justification::centred, false);
}

std::unique_ptr<juce::AccessibilityHandler> CompareButton::createAccessibilityHandler()
{
    return accessibility::handler (
        *this, juce::AccessibilityRole::button, [this] { return letter (side); }, [this] { triggerClick(); });
}

HeaderBar::HeaderBar (PluginProcessor& p) : processor (p), presetBar (p)
{
    presetBar.onEdit = [this] { edited(); };
    presetBar.onIdealWidthChange = [this] { resized(); };
    addAndMakeVisible (presetBar);

    undoButton.setTitle ("Undo");
    redoButton.setTitle ("Redo");
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    for (auto* button : { &undoButton, &redoButton })
    {
        button->setIconSize (18.0f);
        button->setRestColour (colour::text1);
    }
    compareButton.setTooltip ("Switch to B");
    compareButton.onClick = [this] {
        processor.selectCompareSide (otherSide (processor.compareSide()));
        edited();
    };
    copyButton.setInk (colour::text1);
    copyButton.setButtonText ("Copied");
    copyWidth = copyButton.getIdealWidth() + 2 * (chipPadding - 6);
    copyButton.setButtonText ("Copy");
    copyButton.onClick = [this] { copyToOther(); };

    int order = 0;
    for (juce::Button* button : std::initializer_list<juce::Button*> { &undoButton, &redoButton, &compareButton, &copyButton })
    {
        // Tab reaches them, but a click leaves focus where it was, so Delete still reaches the display.
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    // Left to right.
    for (juce::Component* child : std::initializer_list<juce::Component*> { &presetBar, &undoButton, &redoButton, &compareButton, &copyButton })
        child->setExplicitFocusOrder (++order);
    showSide();
    showUndoState();
    startTimer (followTimer, 250);
}

void HeaderBar::timerCallback (int id)
{
    if (id == copiedTimer)
    {
        stopTimer (copiedTimer);
        copyButton.setButtonText ("Copy");
        return;
    }
    showSide();
    showUndoState();
}

void HeaderBar::showSide()
{
    const auto side = processor.compareSide();
    compareButton.showSide (side);
    const auto direction = "Copy " + letter (side) + " to " + letter (otherSide (side));
    copyButton.setTooltip (direction);
    copyButton.setTitle (direction);
}

void HeaderBar::copyToOther()
{
    processor.copyToOther();
    copyButton.setButtonText ("Copied");
    startTimer (copiedTimer, 1000);
    edited();
}

void HeaderBar::edited()
{
    showSide();
    showUndoState();
}

void HeaderBar::undo()
{
    processor.editHistory().undo();
    edited();
}

void HeaderBar::redo()
{
    processor.editHistory().redo();
    edited();
}

void HeaderBar::showUndoState()
{
    undoButton.setEnabled (processor.editHistory().canUndo());
    redoButton.setEnabled (processor.editHistory().canRedo());
}

int HeaderBar::rightGroupWidth() const
{
    return 2 * iconButtonSize + buttonGap + compareMargin + buttonGap + compareButton.getIdealWidth() + buttonGap + copyWidth;
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.setFont (wordmarkFont());
    g.setColour (colour::text1);
    g.drawText (staple::wordmark, wordmark, juce::Justification::centredLeft, false);
}

void HeaderBar::resized()
{
    auto row = getLocalBounds().reduced (sidePadding, 0);
    // The Preset group as wide as it wants, short of crowding the side groups.
    const int roomForCentre = row.getWidth() - 2 * groupGap - 2 * rightGroupWidth();
    constexpr int smallestCentre = 2 * (PresetBar::stepButtonSize + PresetBar::gap) + PresetNameButton::minimumWidth;
    const int centreWidth = juce::jmax (smallestCentre, juce::jmin (presetBar.getIdealWidth(), roomForCentre));
    const int side = (row.getWidth() - centreWidth - 2 * groupGap) / 2;
    auto left = row.removeFromLeft (side);
    wordmark = left.withWidth (juce::jmin (left.getWidth(), widthOf (wordmarkFont(), staple::wordmark) + 1));
    row.removeFromLeft (groupGap);
    presetBar.setBounds (row.removeFromLeft (centreWidth));
    row.removeFromLeft (groupGap);

    const auto place = [&row] (juce::Component& c, int width, int height) {
        c.setBounds (row.removeFromRight (width).withSizeKeepingCentre (width, height));
    };
    place (copyButton, copyWidth, chipHeight);
    row.removeFromRight (buttonGap);
    place (compareButton, compareButton.getIdealWidth(), chipHeight);
    row.removeFromRight (compareMargin + buttonGap);
    place (redoButton, iconButtonSize, iconButtonSize);
    row.removeFromRight (buttonGap);
    place (undoButton, iconButtonSize, iconButtonSize);
}

} // namespace eq1
