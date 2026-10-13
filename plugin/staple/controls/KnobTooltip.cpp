#include "KnobTooltip.h"

#include "../Painting.h"
#include "../Tokens.h"
#include "Knob.h"
#include "TypeIn.h"

namespace staple
{

namespace
{
constexpr int minimumWidth = 86, gap = 6;
constexpr int fieldWidth = 74, fieldHeight = 20;
} // namespace

KnobTooltip::KnobTooltip (Knob& k) : knob (k)
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
        auto bounds = juce::Rectangle<int> (width, height).withCentre ({ area.getCentreX(), 0 });
        bounds.setY (area.getY() - gap - height);
        if (bounds.getY() < 0)
            bounds.setY (area.getBottom() + gap);
        bounds.setX (juce::jlimit (0, std::max (0, layer->getWidth() - width), bounds.getX()));
        setBounds (bounds);
    }
    repaint();
}

void KnobTooltip::appear()
{
    toFront (false);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, tokens::motion::dur1Ms);
    setVisible (true);
}

void KnobTooltip::disappear()
{
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
            safe->knob.hideTooltipUnlessHovered (juce::Desktop::getMousePosition());
    });
}

} // namespace staple
