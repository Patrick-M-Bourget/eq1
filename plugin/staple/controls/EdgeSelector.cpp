#include "EdgeSelector.h"

#include "../Fonts.h"
#include "../LookAndFeel.h"

namespace staple
{

namespace
{
namespace colour = tokens::colour;

constexpr float iconWidth = 20.0f, iconHeight = 12.0f, dotSize = 5.0f;
constexpr float flushPadding = 14.0f, iconGap = 9.0f, dotGap = 8.0f;
// The hairline's alpha at the inner edge, at 45 % of the width from it, and 0 from 90 %.
constexpr float hairlineInner = 0.6f, hairlineMid = 0.18f;
constexpr float washAlpha = 0.12f, washReach = 0.49f; // the wash fades out at this fraction of the width

std::unique_ptr<juce::Drawable> menuIcon (Icon icon)
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath (pathOf (icon));
    drawable->setFill (juce::FillType());
    drawable->setStrokeFill (colour::text1);
    drawable->setStrokeType (juce::PathStrokeType (tokens::size::iconStroke * gridOf (icon).getWidth() / iconWidth,
                                                   juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return drawable;
}
} // namespace

EdgeSelector::EdgeSelector (const juce::String& name, Side s) : juce::ComboBox (name), side (s)
{
    // The face draws the selected item itself, with its icon and dot.
    setColour (juce::ComboBox::textColourId, juce::Colours::transparentBlack);
    setHasFocusOutline (true);
}

void EdgeSelector::setSide (Side newSide)
{
    side = newSide;
    repaint();
}

void EdgeSelector::setEdgeColour (juce::Colour colour)
{
    edgeColour = colour;
    repaint();
}

void EdgeSelector::setIconColour (juce::Colour colour)
{
    iconColour = colour;
    repaint();
}

void EdgeSelector::addItem (const juce::String& text, int itemId, Icon icon, std::optional<juce::Colour> dot)
{
    juce::PopupMenu::Item item (text);
    item.itemID = itemId;
    item.image = menuIcon (icon);
    getRootMenu()->addItem (std::move (item));
    icons[itemId] = icon;
    if (dot.has_value())
        dots[itemId] = *dot;
}

void EdgeSelector::paint (juce::Graphics& g)
{
    const bool left = side == Side::left;
    const auto bounds = getLocalBounds().toFloat();
    if (! isEnabled())
        g.beginTransparencyLayer (tokens::motion::disabledAlpha);

    // Rounded on the inner side only.
    const float r = tokens::size::r3;
    juce::Path shape;
    shape.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), r, r, ! left, left, ! left, left);
    g.setColour (colour::edgeSelectorBase);
    g.fillPath (shape);

    const float innerX = left ? bounds.getRight() : bounds.getX();
    const float towardFlush = left ? -1.0f : 1.0f;
    {
        juce::ColourGradient wash (edgeColour.withAlpha (washAlpha), innerX, bounds.getY(), edgeColour.withAlpha (0.0f),
                                   innerX + towardFlush * washReach * bounds.getWidth(), bounds.getY(), true);
        g.setGradientFill (wash);
        g.fillPath (shape);
    }
    {
        const float w = bounds.getWidth();
        juce::ColourGradient hairline (edgeColour.withAlpha (hairlineInner), innerX, 0.0f, edgeColour.withAlpha (0.0f),
                                       innerX + towardFlush * 0.9f * w, 0.0f, false);
        hairline.addColour (0.45 / 0.9, edgeColour.withAlpha (hairlineMid));
        g.setGradientFill (hairline);
        juce::Path edge;
        const auto line = bounds.reduced (0.5f);
        edge.addRoundedRectangle (line.getX(), line.getY(), line.getWidth(), line.getHeight(), r - 0.5f, r - 0.5f, ! left, left, ! left, left);
        g.strokePath (edge, juce::PathStrokeType (1.0f));
    }

    // The selected item: its icon, its dot and its name, centred beside the flush side's padding.
    const int id = getSelectedId();
    const auto text = getText();
    const auto textFont = font (tokens::size::fs4, Weight::medium);
    const auto icon = icons.find (id);
    const auto dot = dots.find (id);
    float width = juce::GlyphArrangement::getStringWidth (textFont, text);
    if (icon != icons.end())
        width += iconWidth + iconGap;
    if (dot != dots.end())
        width += dotSize + dotGap;
    auto content = bounds;
    if (left)
        content.removeFromLeft (flushPadding);
    else
        content.removeFromRight (flushPadding);
    auto row = content.withSizeKeepingCentre (std::min (width, content.getWidth()), bounds.getHeight());
    if (icon != icons.end())
    {
        drawIcon (g, icon->second, row.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, iconHeight), iconColour);
        row.removeFromLeft (iconGap);
    }
    if (dot != dots.end())
    {
        g.setColour (dot->second);
        g.fillEllipse (row.removeFromLeft (dotSize).withSizeKeepingCentre (dotSize, dotSize));
        row.removeFromLeft (dotGap);
    }
    g.setFont (textFont);
    g.setColour (colour::text1);
    g.drawText (text, row, juce::Justification::centredLeft, true);

    if (! isEnabled())
        g.endTransparencyLayer();
}

bool EdgeSelector::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const int delta = code == juce::KeyPress::upKey || code == juce::KeyPress::leftKey ? -1
                      : code == juce::KeyPress::downKey || code == juce::KeyPress::rightKey ? 1
                                                                                             : 0;
    if (delta == 0 || key.getModifiers().isAnyModifierKeyDown())
        return juce::ComboBox::keyPressed (key);
    if (! isEnabled())
        return false;
    LookAndFeel::keyUsed (*this);
    // At once, rather than in a message posted for later, so each press is its own undo step.
    for (int i = getSelectedItemIndex() + delta; i >= 0 && i < getNumItems(); i += delta)
    {
        if (isItemEnabled (getItemId (i)))
        {
            setSelectedItemIndex (i, juce::sendNotificationSync);
            break;
        }
    }
    return true;
}

} // namespace staple
