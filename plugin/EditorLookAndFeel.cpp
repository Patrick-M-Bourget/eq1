#include "EditorLookAndFeel.h"

namespace eq1
{

namespace
{
constexpr int ringWidth = 2, ringOffset = 2;
} // namespace

std::unique_ptr<juce::FocusOutline> EditorLookAndFeel::createFocusOutlineForComponent (juce::Component&)
{
    struct Ring final : juce::FocusOutline::OutlineWindowProperties
    {
        explicit Ring (const EditorLookAndFeel& owner) : lookAndFeel (owner) {}

        juce::Rectangle<int> getOutlineBounds (juce::Component& focused) override
        {
            return focused.getScreenBounds().expanded (ringWidth + ringOffset);
        }

        void drawOutline (juce::Graphics& g, int width, int height) override
        {
            if (! lookAndFeel.isFocusRingShown())
                return;
            g.setColour (juce::Colour (0xffa0d8ff));
            g.drawRoundedRectangle (juce::Rectangle<int> (width, height).toFloat().reduced (ringWidth * 0.5f), 4.0f, static_cast<float> (ringWidth));
        }

        const EditorLookAndFeel& lookAndFeel;
    };
    return std::make_unique<juce::FocusOutline> (std::make_unique<Ring> (*this));
}

void EditorLookAndFeel::showFocusRing (bool shown)
{
    if (std::exchange (focusRingShown, shown) == shown)
        return;
    // The ring is drawn over the focused component's parent, around it.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent(); focused != nullptr && focused->getParentComponent() != nullptr)
        focused->getParentComponent()->repaint (focused->getBounds().expanded (ringWidth + ringOffset));
}

void EditorLookAndFeel::keyUsed (juce::Component& component)
{
    if (auto* lookAndFeel = dynamic_cast<EditorLookAndFeel*> (&component.getLookAndFeel()))
        lookAndFeel->showFocusRing (true);
}

} // namespace eq1
