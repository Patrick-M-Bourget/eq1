#include "DynamicsSection.h"

#include "Parameters.h"
#include "PluginProcessor.h"
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

// Threshold's top in dB; the fader's position above it is Auto.
constexpr double thresholdTopDb = 0.0;

// The meter's colours up a track of this height, from its bottom.
juce::ColourGradient meterGradient (juce::Rectangle<float> track)
{
    juce::ColourGradient gradient (colour::meter1, 0.0f, track.getBottom(), colour::meterClip, 0.0f, track.getY(), false);
    gradient.addColour (tokens::dynamics::meterStop2, colour::meter2);
    gradient.addColour (tokens::dynamics::meterStop3, colour::meter3);
    return gradient;
}

double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
} // namespace

//==============================================================================
ThresholdFader::ThresholdFader (PluginProcessor& p) : KeyboardSlider ("Threshold"), processor (p)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setHasFocusOutline (true);
    setRange (-60.0, autoPosition, 0.1);
    setDoubleClickReturnValue (true, autoPosition);
    textFromValueFunction = [] (double value) { return isAuto (value) ? juce::String ("Auto") : juce::String (value, 1) + " dB"; };
    valueFromTextFunction = [] (const juce::String& text) {
        return text.trim().equalsIgnoreCase ("Auto") ? autoPosition : juce::jmin (thresholdTopDb, text.getDoubleValue());
    };
    // Between 0 dB and Auto there are no values: a step up from 0 dB is Auto, a step down from Auto 0 dB.
    landStep = [] (double from, double to) {
        if (isAuto (from))
            return to < from ? thresholdTopDb : from;
        return isAuto (to) ? autoPosition : to;
    };
    ballistics.reset();
    level = levelFloorDb;
    lastRead = now();
    startTimerHz (60);
}

juce::Rectangle<float> ThresholdFader::track() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (static_cast<float> (layout::faderTrackWidth), static_cast<float> (getHeight()));
}

float ThresholdFader::thumbCentreY (double position)
{
    const float thumb = layout::faderThumbHeight;
    const auto travel = static_cast<float> (getHeight()) - thumb;
    return thumb / 2.0f + (1.0f - static_cast<float> (valueToProportionOfLength (juce::jlimit (getMinimum(), getMaximum(), position)))) * travel;
}

float ThresholdFader::levelTopY (double levelDb)
{
    if (levelDb < getMinimum())
        return static_cast<float> (getHeight());
    return thumbCentreY (std::min (levelDb, thresholdTopDb));
}

void ThresholdFader::timerCallback()
{
    // A newly metered Band starts from nothing rather than from the last one's falling level.
    if (const int slot = processor.meteredSlot(); slot != meteredSlot)
    {
        meteredSlot = slot;
        ballistics.reset();
    }
    const double time = now();
    const double read = ballistics.update (processor.readDetectionLevel(), time - lastRead);
    lastRead = time;
    if (isShowing() && std::abs (levelTopY (read) - levelTopY (level)) > 0.1f)
        repaint();
    level = read;
}

void ThresholdFader::paint (juce::Graphics& g)
{
    // The track, with the meter's colours faint behind it and the Detection Level filling it.
    const auto bar = track();
    juce::Path shape;
    shape.addRoundedRectangle (bar, tokens::size::r1);
    g.setColour (colour::faderTrack);
    g.fillPath (shape);
    {
        const juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (shape);
        const auto gradient = meterGradient (bar);
        auto faint = gradient;
        faint.multiplyOpacity (tokens::dynamics::meterBehindAlpha);
        g.setGradientFill (faint);
        g.fillRect (bar);
        g.setGradientFill (gradient);
        g.fillRect (bar.withTop (levelTopY (level)));
    }

    // The thumb: "A" at Auto, else a line.
    const float thumbY = thumbCentreY (getValue());
    const auto thumb = juce::Rectangle<float> (static_cast<float> (getWidth()), static_cast<float> (layout::faderThumbHeight))
                           .withCentre ({ static_cast<float> (getWidth()) / 2.0f, thumbY });
    staple::drawSoftShadow (g, thumb, tokens::size::r1, tokens::shadow::thumb);
    g.setGradientFill (juce::ColourGradient::vertical (colour::thumbTop, thumb.getY(), colour::thumbBottom, thumb.getBottom()));
    g.fillRoundedRectangle (thumb, tokens::size::r1);
    g.setColour (colour::thumbHighlight);
    g.fillRect (thumb.withHeight (1.0f).reduced (tokens::size::r1, 0.0f));
    if (isAuto (getValue()))
    {
        g.setColour (colour::text1);
        g.setFont (staple::font (tokens::size::fs1, staple::Weight::semiBold));
        g.drawText ("A", thumb, juce::Justification::centred, false);
    }
    else
    {
        g.setColour (colour::thumbLine);
        g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 2.0f).withCentre (thumb.getCentre()), 1.0f);
    }
}

void ThresholdFader::mouseDown (const juce::MouseEvent& e)
{
    if (isEnabled() && e.mods.isLeftButtonDown())
        startMouseDrag (e);
}

void ThresholdFader::mouseDrag (const juce::MouseEvent& e)
{
    continueMouseDrag (e);
}

void ThresholdFader::mouseUp (const juce::MouseEvent&)
{
    endMouseDrag();
}

double ThresholdFader::valueDraggedBy (double from, float pixels, bool)
{
    const auto travel = static_cast<float> (getHeight() - layout::faderThumbHeight);
    const double value = proportionOfLengthToValue (juce::jlimit (0.0, 1.0, valueToProportionOfLength (from) + pixels / travel));
    // The top step is Auto, whole.
    return isAuto (value) ? autoPosition : value;
}

//==============================================================================
class DynamicsSection::DetectionRangeButton final : public juce::Button
{
public:
    DetectionRangeButton() : juce::Button ("Detection Range")
    {
        setHasFocusOutline (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void setFree (bool isFree)
    {
        if (std::exchange (free, isFree) != isFree)
        {
            repaint();
            if (auto* handler = getAccessibilityHandler())
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        }
    }
    bool isFree() const { return free; }
    juce::String text() const { return free ? "Free" : "Band"; }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float brightness = down ? tokens::motion::pressedBrightness : highlighted ? tokens::motion::hoverBrightness : 1.0f;
        g.setColour (colour::fill1.withMultipliedBrightness (brightness));
        g.fillRoundedRectangle (bounds, tokens::size::r2);
        const auto font = staple::font (tokens::size::fs2, staple::Weight::medium);
        constexpr float iconWidth = 14.0f, iconHeight = 12.0f, gap = 6.0f;
        const float textWidth = juce::GlyphArrangement::getStringWidth (font, text());
        auto content = bounds.withSizeKeepingCentre (iconWidth + gap + textWidth, bounds.getHeight());
        staple::drawIcon (g, free ? staple::Icon::detectionRangeFree : staple::Icon::detectionRangeBand,
                          content.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, iconHeight), colour::text1);
        content.removeFromLeft (gap);
        g.setColour (colour::text1);
        g.setFont (font);
        g.drawText (text(), content, juce::Justification::centredLeft, false);
    }

private:
    // A button read with its value: "Band" or "Free".
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        struct Value final : juce::AccessibilityTextValueInterface
        {
            explicit Value (DetectionRangeButton& b) : button (b) {}
            bool isReadOnly() const override { return true; }
            juce::String getCurrentValueAsString() const override { return button.text(); }
            void setValueAsString (const juce::String&) override {}
            DetectionRangeButton& button;
        };
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
                                                             juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press,
                                                                                                     [this] { triggerClick(); }),
                                                             juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
    }

    bool free = false;
};

//==============================================================================
DynamicsSection::DynamicsSection (PluginProcessor& p)
    : processor (p), threshold (p), detectionRange (std::make_unique<DetectionRangeButton>()), attack (tokens::knob::small, "Attack"),
      release (tokens::knob::small, "Release")
{
    threshold.onDragStart = [this] {
        thresholdDragging = true;
        thresholdAttachment->beginGesture();
        thresholdAutoAttachment->beginGesture();
    };
    threshold.onDragEnd = [this] {
        thresholdAttachment->endGesture();
        thresholdAutoAttachment->endGesture();
        thresholdDragging = false;
    };
    threshold.onValueChange = [this] { storeThreshold(); };
    addAndMakeVisible (threshold);

    // Detection Source: lit while External; a click switches it.
    detectionSource.setToggleable (true);
    detectionSource.setLitColour (colour::text1);
    detectionSource.setIconSize (14.0f);
    detectionSource.onClick = [this] {
        if (detectionSourceAttachment != nullptr)
            detectionSourceAttachment->setValueAsCompleteGesture (detectionSource.getToggleState() ? 0.0f : 1.0f);
        showChoices(); // the attachment doesn't call back for its own change
    };
    // Detection Audition lasts while the button is held, by the mouse or by Space; it lights while it lasts.
    audition.setToggleable (true);
    audition.setIconSize (13.0f);
    audition.onStateChange = [this] {
        if (audition.isDown() && slot != 0)
        {
            processor.setDetectionAudition (slot);
            audition.setToggleState (true, juce::dontSendNotification);
        }
        else if (! audition.isDown())
            releaseAudition();
    };
    detectionRange->onClick = [this] {
        if (detectionRangeAttachment != nullptr)
            detectionRangeAttachment->setValueAsCompleteGesture (detectionRange->isFree() ? 0.0f : 1.0f);
        showChoices();
    };
    for (auto* button : std::initializer_list<juce::Component*> { &detectionSource, &audition, detectionRange.get() })
        addAndMakeVisible (*button);

    const std::pair<juce::Label*, const char*> labels[] = { { &attackLabel, "A" }, { &releaseLabel, "R" } };
    for (auto [label, text] : labels)
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (staple::font (tokens::size::fs1));
        label->setColour (juce::Label::textColourId, colour::text3);
        label->setJustificationType (juce::Justification::centredTop);
        label->setBorderSize ({});
        // The knob is titled with its parameter's name, so a screen reader doesn't stop at the label too.
        label->setAccessible (false);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }
    for (auto* knob : { &attack, &release })
    {
        knob->setBipolar (true);
        addAndMakeVisible (*knob);
    }

    // Tab's order: the fader, then the column top to bottom, left to right.
    int order = 0;
    for (juce::Component* control : std::initializer_list<juce::Component*> { &threshold, &detectionSource, &audition, detectionRange.get(), &attack, &release })
        control->setExplicitFocusOrder (++order);
    setSize (layout::dynamicsSectionWidth, layout::dynamicsSectionHeight);
}

DynamicsSection::~DynamicsSection()
{
    releaseAudition();
}

void DynamicsSection::releaseAudition()
{
    audition.setToggleState (false, juce::dontSendNotification);
    if (processor.detectionAuditionSlot() != 0)
        processor.setDetectionAudition (0);
}

void DynamicsSection::setBandColour (juce::Colour colour)
{
    audition.setLitColour (colour);
    attack.setArcColour (colour);
    release.setArcColour (colour);
}

void DynamicsSection::show (int newSlot)
{
    releaseAudition();
    slot = newSlot;
    attackAttachment.reset();
    releaseAttachment.reset();
    thresholdAttachment.reset();
    thresholdAutoAttachment.reset();
    detectionSourceAttachment.reset();
    detectionRangeAttachment.reset();
    if (slot == 0)
        return;

    auto& state = processor.parameterState();
    attackAttachment = std::make_unique<SliderAttachment> (state, parameters::attackId (slot), attack);
    releaseAttachment = std::make_unique<SliderAttachment> (state, parameters::releaseId (slot), release);
    const auto attach = [&] (const juce::String& id, std::function<void()> shown) {
        return std::make_unique<juce::ParameterAttachment> (*state.getParameter (id), [shown] (float) { shown(); });
    };
    thresholdAttachment = attach (parameters::thresholdId (slot), [this] { showThreshold(); });
    thresholdAutoAttachment = attach (parameters::thresholdAutoId (slot), [this] { showThreshold(); });
    detectionSourceAttachment = attach (parameters::detectionSourceId (slot), [this] { showChoices(); });
    detectionRangeAttachment = attach (parameters::detectionRangeId (slot), [this] { showChoices(); });

    const auto name = [&state] (const juce::String& id) { return state.getParameter (id)->getName (100); };
    attack.describe (*state.getParameter (parameters::attackId (slot)));
    release.describe (*state.getParameter (parameters::releaseId (slot)));
    threshold.setTitle (name (parameters::thresholdId (slot)));
    detectionSource.setTitle (name (parameters::detectionSourceId (slot)));
    detectionRange->setTitle (name (parameters::detectionRangeId (slot)));
    audition.setTitle ("Band " + juce::String (slot) + " Detection Audition");
    showThreshold();
    showChoices();
}

void DynamicsSection::showChoices()
{
    if (slot == 0)
        return;
    auto& state = processor.parameterState();
    const auto chosen = [&state] (const juce::String& id) { return state.getParameter (id)->getValue() >= 0.5f; };
    detectionSource.setToggleState (chosen (parameters::detectionSourceId (slot)), juce::dontSendNotification);
    detectionRange->setFree (chosen (parameters::detectionRangeId (slot)));
}

void DynamicsSection::showThreshold()
{
    if (slot == 0)
        return;
    // From the parameters themselves: while their listeners are told of a change, the raw values may
    // not have caught up yet.
    auto& state = processor.parameterState();
    const auto& level = *state.getParameter (parameters::thresholdId (slot));
    const bool automatic = state.getParameter (parameters::thresholdAutoId (slot))->getValue() >= 0.5f;
    threshold.setValue (automatic ? ThresholdFader::autoPosition : level.convertFrom0to1 (level.getValue()), juce::dontSendNotification);
}

void DynamicsSection::storeThreshold()
{
    if (slot == 0 || thresholdAttachment == nullptr)
        return;
    const double value = threshold.getValue();
    const bool automatic = ThresholdFader::isAuto (value);
    // A drag is one gesture on both parameters; a key step or a typed value is one undo step too.
    if (thresholdDragging)
    {
        thresholdAutoAttachment->setValueAsPartOfGesture (automatic ? 1.0f : 0.0f);
        if (! automatic)
            thresholdAttachment->setValueAsPartOfGesture (static_cast<float> (value));
    }
    else
    {
        processor.editHistory().beginTransaction();
        thresholdAutoAttachment->setValueAsCompleteGesture (automatic ? 1.0f : 0.0f);
        if (! automatic)
            thresholdAttachment->setValueAsCompleteGesture (static_cast<float> (value));
        processor.editHistory().endTransaction();
    }
    // The attachments don't call back for their own changes: a key step shows what was stored, such
    // as Auto at its top position. A mouse drag carries on from where it is.
    if (! threshold.isMouseButtonDown())
        showThreshold();
}

void DynamicsSection::paint (juce::Graphics& g)
{
    g.setColour (colour::surface1);
    g.fillRoundedRectangle (juce::Rectangle<int> (layout::dynamicsSectionWidth, layout::dynamicsSectionHeight).toFloat(), tokens::size::r3);
}

void DynamicsSection::resized()
{
    // Laid out at the section's full width; while it slides open or closed its own bounds clip it.
    auto area = juce::Rectangle<int> (layout::dynamicsSectionWidth, layout::dynamicsSectionHeight)
                    .reduced (layout::dynamicsSectionPaddingX, layout::dynamicsSectionPaddingY);
    threshold.setBounds (area.removeFromLeft (layout::faderWidth).withSizeKeepingCentre (layout::faderWidth, layout::faderTrack));
    area.removeFromLeft (layout::dynamicsSectionGap);
    auto column = area.removeFromLeft (layout::dynamicsColumnWidth);

    // The column, top to bottom with the space between: the two toggles at its right, the Detection
    // Range switch, and Attack and Release with their labels.
    constexpr int icon = layout::dynamicsIcon, iconGap = 4, knobLabelGap = 1, labelHeight = 12, knobGap = 10;
    const int knob = static_cast<int> (tokens::knob::small);
    const int knobRow = knob + knobLabelGap + labelHeight;
    const int spare = (column.getHeight() - icon - layout::detectionRangeButtonHeight - knobRow) / 2;
    auto toggles = column.removeFromTop (icon);
    audition.setBounds (toggles.removeFromRight (icon));
    toggles.removeFromRight (iconGap);
    detectionSource.setBounds (toggles.removeFromRight (icon));
    column.removeFromTop (spare);
    detectionRange->setBounds (column.removeFromTop (layout::detectionRangeButtonHeight));
    auto knobs = column.removeFromBottom (knobRow).withSizeKeepingCentre (2 * knob + knobGap, knobRow);
    const std::pair<staple::Knob*, juce::Label*> columns[] = { { &attack, &attackLabel }, { &release, &releaseLabel } };
    for (auto [k, label] : columns)
    {
        auto place = knobs.removeFromLeft (knob);
        knobs.removeFromLeft (knobGap);
        const int side = k->getIdealSize();
        k->setBounds (juce::Rectangle<int> (side, side).withCentre ({ place.getCentreX(), place.getY() + knob / 2 }));
        label->setBounds (place.withTop (place.getY() + knob + knobLabelGap).withWidth (knob));
    }
}

} // namespace eq1
