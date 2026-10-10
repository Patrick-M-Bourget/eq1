#include "HoverCard.h"

#include "BandEditing.h"
#include "BandMenu.h"
#include "PluginProcessor.h"
#include "staple/Tokens.h"
#include "staple/controls/Overlay.h"

namespace eq1
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = tokens::colour;
namespace card = tokens::hoverCard;

// The columns, inside the padding (HANDOFF.md §5.2): Bypass over the Shape, the values, Delete over ▾.
constexpr int button = 22, leftColumn = 26, rightColumn = button;
} // namespace

HoverCard::HoverCard (PluginProcessor& p, BandEditing& e) : processor (p), editing (e)
{
    setWantsKeyboardFocus (false);
    bypass.setOffLook (true);
    bypass.setRestColour (colour::text2);
    bypass.setIconSize (13.0f);
    bypass.onClick = [this] {
        if (slot != 0)
            editing.setBypass ({ slot }, ! editing.band (slot).bypass);
        refresh();
    };
    deleteButton.setIconSize (10.0f);
    deleteButton.onClick = [this] {
        if (slot == 0)
            return;
        editing.deleteBand (slot);
        hide();
    };
    more.setIconSize (10.0f);
    more.setRestColour (colour::text2);
    more.onClick = [this] { openMenu(); };
    for (auto* b : { &bypass, &deleteButton, &more })
    {
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    // Its controls report entering and leaving it.
    addMouseListener (this, true);
}

HoverCard::~HoverCard() { removeMouseListener (this); }

void HoverCard::show (int newSlot, juce::Point<float> handle, juce::Rectangle<int> within)
{
    const bool changed = newSlot != slot;
    slot = newSlot;
    const auto x = juce::roundToInt (handle.x), y = juce::roundToInt (handle.y);
    above = y - within.getY() > card::roomAbove;
    auto area = juce::Rectangle<int> (card::width, card::height).withCentre ({ x, 0 });
    area.setY (above ? y - card::gap - card::height : y + card::gap);
    area.setX (juce::jlimit (within.getX() + card::inset, std::max (within.getX() + card::inset, within.getRight() - card::inset - card::width),
                             area.getX()));
    const float newTipX = handle.x - static_cast<float> (area.getX() - margin);
    setBounds (area.expanded (margin));
    if (changed || ! juce::approximatelyEqual (newTipX, tipX))
    {
        tipX = newTipX;
        repaint();
    }
    refresh();
    setVisible (true);
}

void HoverCard::hide()
{
    slot = 0;
    pointerOver = false;
    setVisible (false);
}

void HoverCard::refresh()
{
    if (slot == 0)
        return;
    const auto band = "Band " + juce::String (slot);
    setTitle (band + " quick controls");
    bypass.setTitle (band + " Bypass");
    deleteButton.setTitle ("Delete " + band);
    more.setTitle (band + " menu");
    bypass.setToggleState (editing.band (slot).bypass, juce::dontSendNotification);
}

juce::PopupMenu HoverCard::menu()
{
    const juce::Component::SafePointer<HoverCard> safe (this);
    const int shown = slot;
    const BandMenu bandMenu { editing,
                              { shown },
                              processor.isStereoPlacementAvailable(),
                              [safe, shown] {
                                  if (safe != nullptr)
                                  {
                                      safe->editing.deleteBand (shown);
                                      safe->hide();
                                  }
                              },
                              [safe] {
                                  if (safe != nullptr && safe->onSelectAll)
                                      safe->onSelectAll();
                              },
                              [] (std::vector<int>) {},
                              juce::SystemClipboard::getTextFromClipboard(),
                              [] (const juce::String& text) { juce::SystemClipboard::copyTextToClipboard (text); } };
    return bandMenu.build();
}

void HoverCard::openMenu()
{
    if (slot == 0)
        return;
    menuOpen = true;
    auto popup = menu();
    // A menu is a window of its own: it draws with the editor's look only when given it.
    popup.setLookAndFeel (&getLookAndFeel());
    const juce::Component::SafePointer<HoverCard> safe (this);
    popup.showMenuAsync (juce::PopupMenu::Options().withDeletionCheck (*this).withTargetComponent (&more), [safe] (int) {
        if (safe != nullptr)
            safe->menuOpen = false;
    });
}

void HoverCard::resized()
{
    auto area = bodyArea().reduced (card::padding);
    auto left = area.removeFromLeft (leftColumn);
    auto right = area.removeFromRight (rightColumn);
    bypass.setBounds (left.removeFromTop (button).withSizeKeepingCentre (button, button));
    deleteButton.setBounds (right.removeFromTop (button));
    more.setBounds (right.removeFromBottom (button));
}

void HoverCard::paint (juce::Graphics& g)
{
    const auto body = bodyArea().toFloat();
    staple::drawSoftShadow (g, body, tokens::size::r3, tokens::shadow::shadow1);
    const auto border = colour::hoverCardLine;
    g.setColour (colour::menu);
    g.fillRoundedRectangle (body, tokens::size::r3);
    g.setColour (border);
    g.drawRoundedRectangle (body.reduced (0.5f), tokens::size::r3, 1.0f);

    // The tip: a square turned 45° on the edge facing the handle, half of it showing, its two outer edges
    // bordered and the body's border under it covered.
    const float reach = card::tip / juce::MathConstants<float>::sqrt2;
    const float edge = above ? body.getBottom() - 0.5f : body.getY() + 0.5f;
    const float direction = above ? 1.0f : -1.0f;
    juce::Path tip;
    tip.startNewSubPath (tipX - reach, edge - direction);
    tip.lineTo (tipX - reach, edge);
    tip.lineTo (tipX, edge + direction * reach);
    tip.lineTo (tipX + reach, edge);
    tip.lineTo (tipX + reach, edge - direction);
    tip.closeSubPath();
    g.setColour (colour::menu);
    g.fillPath (tip);
    juce::Path outline;
    outline.startNewSubPath (tipX - reach, edge);
    outline.lineTo (tipX, edge + direction * reach);
    outline.lineTo (tipX + reach, edge);
    g.setColour (border);
    g.strokePath (outline, juce::PathStrokeType (1.0f));
}

bool HoverCard::hitTest (int x, int y) { return bodyArea().contains (x, y); }

void HoverCard::mouseEnter (const juce::MouseEvent&) { pointerOver = true; }

void HoverCard::mouseExit (const juce::MouseEvent& e)
{
    // Leaving one of its controls for the card itself is still on it.
    pointerOver = bodyArea().contains (e.getEventRelativeTo (this).position.toInt());
}

std::unique_ptr<juce::AccessibilityHandler> HoverCard::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

} // namespace eq1
