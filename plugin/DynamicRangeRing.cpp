#include "DynamicRangeRing.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/LookAndFeel.h"
#include "staple/Tokens.h"

#include <cmath>

namespace eq1
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = tokens::colour;

// Where the knob shows a dB value, as an angle clockwise from 12 o'clock: clipped to its sweep.
float angleOn (staple::Knob& knob, double db)
{
    const double proportion = knob.valueToProportionOfLength (juce::jlimit (knob.getMinimum(), knob.getMaximum(), db));
    return juce::degreesToRadians (tokens::knob::sweepDegrees * (static_cast<float> (proportion) - 0.5f));
}

juce::ColourGradient radial (juce::Point<float> centre, float inner, float outer, juce::Colour innerColour, juce::Colour outerColour)
{
    juce::ColourGradient gradient (innerColour, centre, outerColour, centre.translated (outer, 0.0f), true);
    gradient.addColour (inner / outer, innerColour);
    return gradient;
}

// The live Gain arc hides below this movement, and repaints only past this much change.
constexpr double repaintStepDb = 0.01;
} // namespace

DynamicRangeRing::DynamicRangeRing (PluginProcessor& p, staple::Knob& g) : KeyboardSlider ("Dynamic Range"), processor (p), gain (g)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setInterceptsMouseClicks (false, false);
    setArrowSteps (1.0, 0.5);
    gain.setRing (this);
    gain.addKeyListener (this);
}

DynamicRangeRing::~DynamicRangeRing()
{
    drag.reset();
    gain.removeKeyListener (this);
    gain.setRing (nullptr);
}

void DynamicRangeRing::show (int newSlot)
{
    drag.reset();
    slot = newSlot;
    attachment.reset();
    liveGain.reset();
    if (slot == 0)
    {
        stopTimer();
        return;
    }
    auto& state = processor.parameterState();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameters::dynamicRangeId (slot), *this);
    describe (*state.getParameter (parameters::dynamicRangeId (slot)));
    startTimerHz (60);
    gain.ringChanged();
}

void DynamicRangeRing::setAvailable (bool available)
{
    if (available == isVisible())
        return;
    if (! available)
        drag.reset();
    setVisible (available);
    gain.setRing (available ? this : nullptr);
}

bool DynamicRangeRing::parameterIsOn (const juce::String& id) const
{
    return processor.parameterState().getRawParameterValue (id)->load() >= 0.5f;
}

float DynamicRangeRing::rangeAlpha() const
{
    return slot != 0 && parameterIsOn (parameters::dynamicsBypassId (slot)) ? tokens::knob::ringRangeBypassedAlpha : tokens::knob::ringRangeAlpha;
}

//==============================================================================
void DynamicRangeRing::paintRing (juce::Graphics& g, staple::Knob& knob, juce::Point<float> centre, float inner, float outer)
{
    const float radius = (inner + outer) / 2.0f, width = outer - inner;
    const auto stroke = juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt);
    const auto arc = [&] (float from, float to) {
        juce::Path path;
        path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, std::min (from, to), std::max (from, to), true);
        return path;
    };
    const float sweep = juce::degreesToRadians (tokens::knob::sweepDegrees / 2.0f);

    // The lane.
    g.setColour (colour::knobRingLane);
    g.strokePath (arc (-sweep, sweep), stroke);
    if (slot == 0)
        return;

    // The range, and the Live Gain's movement over it.
    const double gainDb = knob.getValue(), range = getValue();
    const float gainAngle = angleOn (knob, gainDb), rangeAngle = angleOn (knob, gainDb + range);
    if (range != 0.0 && ! juce::approximatelyEqual (gainAngle, rangeAngle))
    {
        auto gradient = radial (centre, inner, outer, colour::dynRangeInner, colour::dynRange);
        gradient.multiplyOpacity (rangeAlpha());
        g.setGradientFill (gradient);
        g.strokePath (arc (gainAngle, rangeAngle), stroke);
    }
    if (liveGain.has_value())
    {
        g.setGradientFill (radial (centre, inner, outer, colour::dynLiveInner, colour::dynLive));
        g.strokePath (arc (gainAngle, angleOn (knob, *liveGain)), stroke);
    }

    // The ▲▼ hint at the range's end, pointing along the ring.
    juce::Path hint;
    hint.addTriangle (-2.4f, -1.2f, 2.4f, -1.2f, 0.0f, -4.4f);
    hint.addTriangle (-2.4f, 1.2f, 2.4f, 1.2f, 0.0f, 4.4f);
    const float end = range != 0.0 ? rangeAngle : gainAngle;
    g.setColour (colour::ringHint);
    g.fillPath (hint, juce::AffineTransform::rotation (end + juce::MathConstants<float>::halfPi)
                          .translated (centre.x + radius * std::sin (end), centre.y - radius * std::cos (end)));

    // Its own focus ring, round the lane, while it has keyboard focus.
    if (hasKeyboardFocus (false))
        if (auto* staple = dynamic_cast<staple::LookAndFeel*> (&getLookAndFeel()); staple != nullptr && staple->isFocusRingShown())
        {
            g.setColour (colour::focus);
            g.drawEllipse (juce::Rectangle<float> (2.0f * outer, 2.0f * outer).withCentre (centre).expanded (tokens::size::focusWidth / 2.0f),
                           tokens::size::focusWidth);
        }
}

void DynamicRangeRing::ringMouseDown (staple::Knob&, const juce::MouseEvent& e)
{
    if (slot == 0)
        return;
    drag.reset (new Drag { e.position.y, getValue(), e.mods.isShiftDown(), ScopedDragNotification (*this) });
}

void DynamicRangeRing::ringMouseDrag (staple::Knob&, const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;
    // Shift pressed or let go mid-drag carries on from where it is, at the new speed.
    if (e.mods.isShiftDown() != drag->fine)
    {
        drag->fine = e.mods.isShiftDown();
        drag->startY = e.position.y;
        drag->startValue = getValue();
    }
    const float pixels = drag->fine ? tokens::knob::fineDragPixels : tokens::knob::dragPixels;
    const double value = drag->startValue + (drag->startY - e.position.y) / pixels * tokens::knob::ringDbPerDrag;
    setValue (juce::jlimit (getMinimum(), getMaximum(), std::round (value * 2.0) / 2.0), juce::sendNotificationSync);
}

void DynamicRangeRing::ringMouseUp (staple::Knob&, const juce::MouseEvent&)
{
    drag.reset();
}

void DynamicRangeRing::ringDoubleClick (staple::Knob&, const juce::MouseEvent& e)
{
    if (slot == 0)
        return;
    // Inside the second press's gesture if it is open, so it is one undo step either way.
    std::optional<ScopedDragNotification> gesture;
    if (drag == nullptr)
        gesture.emplace (*this);
    setValue (0.0, juce::sendNotificationSync);
    if (drag != nullptr)
    {
        drag->startY = e.position.y;
        drag->startValue = 0.0;
    }
}

juce::String DynamicRangeRing::ringTitle() { return getTitle(); }

juce::String DynamicRangeRing::ringValue() { return spokenValue != nullptr ? spokenValue (getValue()) : getTextFromValue (getValue()); }

//==============================================================================
bool DynamicRangeRing::keyPressed (const juce::KeyPress& key)
{
    return KeyboardSlider::keyPressed (key);
}

bool DynamicRangeRing::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const auto mods = key.getModifiers();
    if (! mods.isAltDown() || ! isVisible() || slot == 0)
        return false;
    return KeyboardSlider::keyPressed (juce::KeyPress (key.getKeyCode(), mods.withoutFlags (juce::ModifierKeys::altModifier), key.getTextCharacter()));
}

bool DynamicRangeRing::keyStateChanged (bool isKeyDown, juce::Component*)
{
    if (! isKeyDown)
        KeyboardSlider::keyStateChanged (false);
    return false;
}

void DynamicRangeRing::focusGained (FocusChangeType cause)
{
    KeyboardSlider::focusGained (cause);
    gain.repaint();
}

void DynamicRangeRing::focusLost (FocusChangeType cause)
{
    KeyboardSlider::focusLost (cause);
    gain.repaint();
}

void DynamicRangeRing::valueChanged()
{
    gain.ringChanged();
}

void DynamicRangeRing::timerCallback()
{
    // A held Alt+arrow on the Gain knob ends when the knob loses focus.
    if (! gain.hasKeyboardFocus (false) && ! hasKeyboardFocus (false))
        KeyboardSlider::keyStateChanged (false);

    std::optional<double> live;
    if (slot != 0 && isVisible() && getValue() != 0.0 && ! parameterIsOn (parameters::dynamicsBypassId (slot))
        && ! parameterIsOn (parameters::bypassId (slot)) && ! parameterIsOn (parameters::globalBypassId))
    {
        // Live Gain is heard under Gain Scale, the ring shows stored dB: its movement from the heard
        // Gain, unscaled, is drawn from the stored Gain.
        const double scale = processor.parameterState().getRawParameterValue (parameters::gainScaleId)->load() / 100.0;
        const double movement = processor.liveGainDb (slot) - gain.getValue() * scale;
        if (processor.hasProcessedAudio() && scale > 0.0 && std::abs (movement) >= tokens::knob::liveGainMinimum)
            live = gain.getValue() + movement / scale;
    }
    const bool changed = live.has_value() != liveGain.has_value() || (live.has_value() && std::abs (*live - *liveGain) > repaintStepDb);
    if (changed)
    {
        liveGain = live;
        gain.repaint();
    }
}

} // namespace eq1
