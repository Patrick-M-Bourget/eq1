#include "DetectionRangeBar.h"

#include "BandEditing.h"
#include "BandPanel.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "display/DisplayGeometry.h"
#include "staple/Fonts.h"
#include "staple/Tokens.h"
#include "staple/controls/Overlay.h"

#include <cmath>

namespace eq1
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = tokens::colour;
namespace layout = tokens::layout;

// "120 Hz", "4.50 kHz".
juce::String frequencyText (double frequency)
{
    return frequency >= 1000.0 ? juce::String (frequency / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (frequency)) + " Hz";
}

// Room round a pill handle for its shadow and focus ring.
constexpr int handleMargin = 6;
constexpr float nudgeOctaves = 1.0f / 6.0f;
} // namespace

//==============================================================================
DetectionRangeBar::Handle::Handle (DetectionRangeBar& b, bool lowLimit)
    : KeyboardSlider (lowLimit ? "Detection Low" : "Detection High"), bar (b), isLow (lowLimit)
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setHasFocusOutline (true);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    // A nudge is 1/6 octave towards the key, within the bar's limits.
    landStep = [this] (double from, double to) {
        const double moved = from * std::pow (2.0, (to > from ? 1.0 : -1.0) * nudgeOctaves);
        const double other = (isLow ? bar.high : bar.low)->getValue();
        return isLow ? clampLow (moved, other) : clampHigh (moved, other);
    };
    onValueChange = [this] { bar.place(); };
}

bool DetectionRangeBar::Handle::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::upKey) || key.isKeyCode (juce::KeyPress::downKey))
        return false;
    return KeyboardSlider::keyPressed (key);
}

void DetectionRangeBar::Handle::paint (juce::Graphics& g)
{
    const auto pill = getLocalBounds().toFloat().reduced (static_cast<float> (handleMargin));
    const float radius = pill.getWidth() / 2.0f;
    staple::drawSoftShadow (g, pill, radius, tokens::shadow::thumb);
    g.setGradientFill (juce::ColourGradient::vertical (colour::pillTop, pill.getY(), colour::pillBottom, pill.getBottom()));
    g.fillRoundedRectangle (pill, radius);
    g.setColour (colour::thumbHighlight);
    g.fillRect (pill.withHeight (1.0f).reduced (radius / 2.0f, 0.0f));
}

void DetectionRangeBar::Handle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
        gesture = std::make_unique<ScopedDragNotification> (*this);
}

void DetectionRangeBar::Handle::mouseDrag (const juce::MouseEvent& e)
{
    if (gesture == nullptr)
        return;
    const double frequency = bar.frequencyAt (e.getEventRelativeTo (&bar).position.x);
    setValue (isLow ? clampLow (frequency, bar.high->getValue()) : clampHigh (frequency, bar.low->getValue()), juce::sendNotificationSync);
}

void DetectionRangeBar::Handle::mouseUp (const juce::MouseEvent&)
{
    gesture.reset();
}

//==============================================================================
DetectionRangeBar::Segment::Segment (DetectionRangeBar& b) : bar (b)
{
    setName ("Detection Range");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::String DetectionRangeBar::Segment::valueText() const
{
    return frequencyText (bar.low->getValue()) + juce::String::fromUTF8 (" \xe2\x80\x93 ") + frequencyText (bar.high->getValue());
}

void DetectionRangeBar::Segment::paint (juce::Graphics& g)
{
    g.setColour (colour);
    g.fillRoundedRectangle (getLocalBounds().toFloat().withSizeKeepingCentre (static_cast<float> (getWidth()), layout::segmentHeight), 2.0f);
}

void DetectionRangeBar::Segment::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isLeftButtonDown())
        return;
    startFrequency = bar.frequencyAt (e.getEventRelativeTo (&bar).position.x);
    startLow = bar.low->getValue();
    startHigh = bar.high->getValue();
    // Both limits in one gesture each, overlapping: one undo step.
    lowGesture = std::make_unique<juce::Slider::ScopedDragNotification> (*bar.low);
    highGesture = std::make_unique<juce::Slider::ScopedDragNotification> (*bar.high);
}

void DetectionRangeBar::Segment::mouseDrag (const juce::MouseEvent& e)
{
    if (lowGesture == nullptr)
        return;
    const auto [lowLimit, highLimit]
        = DetectionRangeBar::moved (startLow, startHigh, bar.frequencyAt (e.getEventRelativeTo (&bar).position.x) / startFrequency);
    bar.low->setValue (lowLimit, juce::sendNotificationSync);
    bar.high->setValue (highLimit, juce::sendNotificationSync);
}

void DetectionRangeBar::Segment::mouseUp (const juce::MouseEvent&)
{
    lowGesture.reset();
    highGesture.reset();
}

std::unique_ptr<juce::AccessibilityHandler> DetectionRangeBar::Segment::createAccessibilityHandler()
{
    struct Value final : juce::AccessibilityTextValueInterface
    {
        explicit Value (Segment& s) : segment (s) {}
        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override { return segment.valueText(); }
        void setValueAsString (const juce::String&) override {}
        Segment& segment;
    };
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group, juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
}

//==============================================================================
DetectionRangeBar::DetectionRangeBar (PluginProcessor& p, BandEditing& e, BandPanel& b)
    : processor (p), editing (e), panel (b), low (std::make_unique<Handle> (*this, true)), high (std::make_unique<Handle> (*this, false)),
      segment (std::make_unique<Segment> (*this))
{
    setInterceptsMouseClicks (false, true);
    addAndMakeVisible (*segment);
    addAndMakeVisible (*low);
    addAndMakeVisible (*high);
    low->setExplicitFocusOrder (1);
    high->setExplicitFocusOrder (2);
    setVisible (false);
    panel.addComponentListener (this);
    startTimerHz (30);
}

DetectionRangeBar::~DetectionRangeBar()
{
    panel.removeComponentListener (this);
}

double DetectionRangeBar::clampLow (double low, double high)
{
    return juce::jlimit (lowestLimit, std::max (lowestLimit, std::min (highestLimit, high / minimumRatio)), low);
}

double DetectionRangeBar::clampHigh (double high, double low)
{
    return juce::jlimit (std::min (highestLimit, std::max (lowestLimit, low * minimumRatio)), highestLimit, high);
}

std::pair<double, double> DetectionRangeBar::moved (double low, double high, double ratio)
{
    low *= ratio;
    high *= ratio;
    if (low < lowestLimit)
    {
        high *= lowestLimit / low;
        low = lowestLimit;
    }
    if (high > highestLimit)
    {
        low *= highestLimit / high;
        high = highestLimit;
    }
    return { std::max (low, lowestLimit), high };
}

float DetectionRangeBar::xOf (double frequency) const
{
    return display::DisplayGeometry { .width = getWidth(), .height = 1 }.xOf (frequency);
}

double DetectionRangeBar::frequencyAt (float x) const
{
    return display::DisplayGeometry { .width = getWidth(), .height = 1 }.frequencyAt (x);
}

float DetectionRangeBar::lineY() const { return static_cast<float> (layout::detectionColumnHeight); }

void DetectionRangeBar::setDisplayBounds (juce::Rectangle<int> d)
{
    display = d;
    update();
}

void DetectionRangeBar::componentMovedOrResized (juce::Component&, bool, bool) { update(); }

void DetectionRangeBar::componentVisibilityChanged (juce::Component&) { update(); }

void DetectionRangeBar::timerCallback() { update(); }

void DetectionRangeBar::update()
{
    const int shown = panel.isVisible() ? panel.shownSlot() : 0;
    if (shown != slot)
    {
        slot = shown;
        lowAttachment.reset();
        highAttachment.reset();
        if (slot != 0)
        {
            auto& state = processor.parameterState();
            lowAttachment = std::make_unique<SliderAttachment> (state, parameters::detectionLowId (slot), *low);
            highAttachment = std::make_unique<SliderAttachment> (state, parameters::detectionHighId (slot), *high);
            low->describe (*state.getParameter (parameters::detectionLowId (slot)));
            high->describe (*state.getParameter (parameters::detectionHighId (slot)));
            segment->setTitle ("Band " + juce::String (slot) + " Detection Range");
            segment->colour = tokens::band[static_cast<size_t> (slot - 1)];
        }
    }
    bool free = false;
    if (slot != 0)
    {
        const auto band = editing.band (slot);
        free = isDynamic (band) && ! band.bypass && band.detectionRange == DetectionRange::Free;
    }
    // Over the display, its line detectionRangeBarAbovePanel above the panel's top.
    const int line = panel.getY() - layout::detectionRangeBarAbovePanel;
    const auto bounds = juce::Rectangle<int> (display.getX(), line - layout::detectionColumnHeight, display.getWidth(),
                                              layout::detectionColumnHeight + layout::pillHeight / 2 + handleMargin);
    if (bounds != getBounds())
        setBounds (bounds);
    if (free != isVisible())
        setVisible (free);
}

void DetectionRangeBar::resized() { place(); }

void DetectionRangeBar::place()
{
    const float y = lineY();
    const float x1 = xOf (low->getValue()), x2 = xOf (high->getValue());
    const auto handleAt = [y] (float x) {
        return juce::Rectangle<float> (layout::pillWidth + 2.0f * handleMargin, layout::pillHeight + 2.0f * handleMargin)
            .withCentre ({ x, y })
            .toNearestInt();
    };
    low->setBounds (handleAt (x1));
    high->setBounds (handleAt (x2));
    const float left = std::min (x1, x2), width = std::max (4.0f, std::abs (x2 - x1));
    segment->setBounds (juce::Rectangle<float> (left, y - layout::segmentHitHeight / 2.0f, width, static_cast<float> (layout::segmentHitHeight))
                            .toNearestInt());
    repaint();
}

void DetectionRangeBar::paint (juce::Graphics& g)
{
    if (slot == 0)
        return;
    const float y = lineY();
    const auto band = tokens::band[static_cast<size_t> (slot - 1)];
    const float x1 = xOf (low->getValue()), x2 = xOf (high->getValue());
    const float left = std::min (x1, x2), width = std::max (4.0f, std::abs (x2 - x1));

    // The column of the Band's colour, fading upward from the segment.
    const auto column = juce::Rectangle<float> (left, y - layout::detectionColumnHeight, width, static_cast<float> (layout::detectionColumnHeight));
    const auto tint = band.withAlpha (tokens::dynamics::columnAlpha);
    g.setGradientFill (juce::ColourGradient::vertical (tint.withAlpha (0.0f), column.getY(), tint, column.getBottom()));
    g.fillRect (column);

    // The line across the display.
    g.setColour (colour::line3);
    g.fillRect (0.0f, y - 0.5f, static_cast<float> (getWidth()), 1.0f);

    // Each limit's Frequency above its handle.
    g.setColour (colour::text3);
    g.setFont (staple::font (tokens::size::fs1));
    for (auto [x, value] : { std::pair { x1, low->getValue() }, std::pair { x2, high->getValue() } })
        g.drawText (frequencyText (value), juce::Rectangle<float> (80.0f, 12.0f).withCentre ({ x, y - layout::detectionLabelAbove + 6.0f }),
                    juce::Justification::centred, false);
}

} // namespace eq1
