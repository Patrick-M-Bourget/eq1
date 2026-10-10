#include "Popover.h"

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
constexpr int frameMs = 16;
} // namespace

Popover::Popover()
{
    setVisible (false);
    // Tab moves among its contents while it is open.
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
}

Popover::~Popover()
{
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

    // Focus moves to its first control, or to the popover itself when it has none, so Esc reaches it.
    const auto first = createKeyboardFocusTraverser()->getDefaultComponent (this);
    setWantsKeyboardFocus (first == nullptr);
    if (first != nullptr)
        first->grabKeyboardFocus();
    else
        grabKeyboardFocus();

    openedAt = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimer (frameMs);
}

void Popover::close()
{
    if (! isOpen())
        return;
    auto returnTo = opener;
    opener = nullptr;
    stopTimer();
    juce::Desktop::getInstance().removeGlobalMouseListener (this);
    setTransform ({});
    setAlpha (1.0f);
    setVisible (false);
    if (auto* layer = getParentComponent())
        layer->removeChildComponent (this);
    if (returnTo != nullptr && returnTo->isShowing())
        returnTo->grabKeyboardFocus();
    if (onClose != nullptr)
        onClose();
}

void Popover::timerCallback()
{
    const float t = static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - openedAt) / motion::dur2Ms);
    const float progress = ease (t);
    const float scale = motion::popInScale + (1.0f - motion::popInScale) * progress;
    const auto centre = getCardBounds().toFloat().getCentre();
    setTransform (juce::AffineTransform::scale (scale, scale, centre.x, centre.y)
                      .translated (0.0f, motion::popInOffset * (1.0f - progress)));
    setAlpha (progress);
    if (t >= 1.0f)
    {
        stopTimer();
        setTransform ({});
        setAlpha (1.0f);
    }
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
