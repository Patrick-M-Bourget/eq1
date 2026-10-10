#include "Popover.h"

#include "../LookAndFeel.h"
#include "../Tokens.h"
#include "Overlay.h"

namespace staple
{

namespace
{
namespace colour = tokens::colour;
namespace motion = tokens::motion;

// Room around the card for shadow1 (24 px blur, 8 px down) to show in.
const juce::BorderSize<int> shadowMargin { 4, 12, 20, 12 };
constexpr int gapToOpener = 6;
} // namespace

Popover::Popover() : opening (motion::dur2Ms, 1.0f)
{
    setVisible (false);
    opening.apply = [this] (float progress) { popIn (progress); };
}

Popover::~Popover()
{
    if (escapeFrom != nullptr)
        escapeFrom->removeKeyListener (this);
    juce::Desktop::getInstance().removeGlobalMouseListener (this);
}

void Popover::setCardSize (int width, int height)
{
    setSize (width + shadowMargin.getLeftAndRight(), height + shadowMargin.getTopAndBottom());
}

juce::Rectangle<int> Popover::getCardBounds() const { return shadowMargin.subtractedFrom (getLocalBounds()); }

void Popover::open (juce::Component& newOpener, Placement preferred)
{
    if (isOpen())
        close();
    auto& layer = overlayLayerFor (newOpener);
    if (&layer == &newOpener)
        return;
    opener = &newOpener;
    layer.addChildComponent (*this);

    const auto anchor = layer.getLocalArea (&newOpener, newOpener.getLocalBounds());
    const auto card = getCardBounds();
    const int belowY = anchor.getBottom() + gapToOpener;
    const int aboveY = anchor.getY() - gapToOpener - card.getHeight();
    const bool roomBelow = belowY + card.getHeight() <= layer.getHeight();
    const bool roomAbove = aboveY >= 0;
    const bool below = preferred == Placement::below ? (roomBelow || ! roomAbove) : ! roomAbove && roomBelow;
    const int cardX = juce::jlimit (0, std::max (0, layer.getWidth() - card.getWidth()), anchor.getX());
    setTopLeftPosition (cardX - shadowMargin.getLeft(), (below ? belowY : aboveY) - shadowMargin.getTop());

    toFront (false);
    setVisible (true);
    juce::Desktop::getInstance().addGlobalMouseListener (this);

    // Opened from the keyboard, focus moves to its first control, or to the popover itself when it has
    // none. Opened by a click, focus stays where it was, and Esc reaches it from there.
    const auto* lookAndFeel = dynamic_cast<LookAndFeel*> (&newOpener.getLookAndFeel());
    if (lookAndFeel != nullptr && lookAndFeel->isFocusRingShown())
    {
        const auto first = createKeyboardFocusTraverser()->getDefaultComponent (this);
        setWantsKeyboardFocus (first == nullptr);
        if (first != nullptr)
            first->grabKeyboardFocus();
        else
            grabKeyboardFocus();
    }
    if (auto* top = newOpener.getTopLevelComponent())
    {
        escapeFrom = top;
        top->addKeyListener (this);
    }

    opening.jump (0.0f);
    opening.towards (1.0f);
}

void Popover::close()
{
    if (! isOpen())
        return;
    auto returnTo = opener;
    opener = nullptr;
    // Focus inside it goes back to the opener; focus elsewhere stays there.
    auto* focused = getCurrentlyFocusedComponent();
    const bool focusInside = focused != nullptr && (focused == this || isParentOf (focused));
    juce::Desktop::getInstance().removeGlobalMouseListener (this);
    opening.jump (1.0f);
    setVisible (false);
    if (escapeFrom != nullptr)
        escapeFrom->removeKeyListener (this);
    escapeFrom = nullptr;
    if (auto* layer = getParentComponent())
        layer->removeChildComponent (this);
    if (focusInside && returnTo != nullptr && returnTo->isShowing())
        returnTo->grabKeyboardFocus();
    if (onClose != nullptr)
        onClose();
}

void Popover::popIn (float progress)
{
    setTransform (popInTransform (getCardBounds().toFloat(), progress));
    setAlpha (progress);
}

void Popover::paint (juce::Graphics& g)
{
    const auto card = getCardBounds().toFloat();
    const float r = tokens::size::r3;
    drawSoftShadow (g, card, r, tokens::shadow::shadow1);
    g.setColour (colour::menu);
    g.fillRoundedRectangle (card, r);
    g.setColour (colour::line2);
    g.drawRoundedRectangle (card.reduced (0.5f), r, 1.0f);
}

bool Popover::hitTest (int x, int y) { return getCardBounds().contains (x, y); }

bool Popover::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey || ! isOpen())
        return false;
    close();
    return true;
}

bool Popover::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

void Popover::mouseDown (const juce::MouseEvent& event)
{
    auto* clicked = event.eventComponent;
    if (! isOpen() || clicked == nullptr || clicked == this || isParentOf (clicked))
        return;
    // Its opener toggles it itself.
    if (opener != nullptr && (clicked == opener.getComponent() || opener->isParentOf (clicked)))
        return;
    close();
}

} // namespace staple
