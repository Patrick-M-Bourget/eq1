#include "KnobTooltip.h"

#include "../Painting.h"
#include "../Tokens.h"
#include "Knob.h"
#include "Overlay.h"
#include "TypeIn.h"

namespace staple
{

namespace
{
constexpr int minimumWidth = 86;
constexpr int fieldWidth = 74, fieldHeight = 20;
} // namespace

KnobTooltip::KnobTooltip (Knob& k)
    : knob (k),
      watchdog ([this] (juce::Point<float> pointer) {
          if (! knob.isMouseButtonDown())
              knob.hideTooltipUnlessHovered (pointer.roundToInt());
      })
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

KnobTooltip::~KnobTooltip() = default;

void KnobTooltip::refresh()
{
    title = knob.tooltipTitle();
    value = knob.tooltipValue();
    const int width = std::max (minimumWidth, static_cast<int> (readoutCardWidth (title, value)));
    const int height = readoutCard::height;

    if (auto* layer = getParentComponent())
    {
        const float r = knob.getFaceRadius();
        const auto face = juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (knob.getFaceCentre());
        const auto area = layer->getLocalArea (&knob, face).toNearestInt();
        // Over the label, from its top; just under the face without one.
        auto* label = knob.getLabel();
        const auto top = label != nullptr && label->isShowing() ? std::optional<int> (layer->getLocalArea (label, label->getLocalBounds()).getY())
                                                                : std::nullopt;
        setBounds (placeCardBy (area, { width, height }, layer->getLocalBounds(), top));
    }
    repaint();
}

void KnobTooltip::appear()
{
    toFront (false);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, tokens::motion::dur1Ms);
    setVisible (true);
    watchdog.start();
}

void KnobTooltip::disappear()
{
    watchdog.stop();
    if (isEditing())
        stopEditing (false);
    juce::Desktop::getInstance().getAnimator().cancelAnimation (this, false);
    setVisible (false);
}

void KnobTooltip::paint (juce::Graphics& g)
{
    drawReadoutCard (g, getLocalBounds().toFloat(), title, isEditing() ? juce::String() : value);
}

void KnobTooltip::resized()
{
    if (editor != nullptr)
        editor->setBounds (juce::Rectangle<int> (fieldWidth, fieldHeight)
                               .withCentre ({ getWidth() / 2, readoutCard::paddingTop + readoutCard::titleLine + 1 + readoutCard::valueLine / 2 }));
}

void KnobTooltip::mouseDoubleClick (const juce::MouseEvent&)
{
    if (knob.isEnabled())
        startEditing();
}

void KnobTooltip::mouseExit (const juce::MouseEvent& e)
{
    if (! isEditing() && ! knob.isMouseButtonDown())
        knob.hideTooltipUnlessHovered (e.getScreenPosition());
}

void KnobTooltip::startEditing()
{
    if (isEditing())
        return;
    // The value as typed: without its unit.
    editor = makeTypeInField (title + " value", knob.textToType (value), [this] { stopEditing (true); }, [this] { stopEditing (false); });
    addAndMakeVisible (*editor);
    resized();
    repaint();
    focusTypeInField (*editor);
}

void KnobTooltip::stopEditing (bool commit)
{
    if (editor == nullptr)
        return;
    const auto text = closeTypeInField (editor);
    if (commit)
        knob.commitTypedText (text);
    refresh();
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<KnobTooltip> (this)] {
        if (safe != nullptr && ! safe->isEditing() && ! safe->isMouseOver (true))
            safe->knob.hideTooltipUnlessHovered (TipWatchdog::pointer().roundToInt());
    });
}

} // namespace staple
