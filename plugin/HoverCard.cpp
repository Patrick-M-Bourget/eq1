#include "HoverCard.h"

#include "BandEditing.h"
#include "BandMenu.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "ShapeIcon.h"
#include "staple/Fonts.h"
#include "staple/Tokens.h"
#include "staple/controls/Knob.h"
#include "staple/controls/Overlay.h"
#include "staple/controls/TypeIn.h"

#include <algorithm>

namespace eq1
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = tokens::colour;
namespace card = tokens::hoverCard;

// The columns, inside the padding (HANDOFF.md §5.2): Bypass over the Shape, the values, Delete over ▾.
constexpr int button = 22, leftColumn = 26, rightColumn = button, valuesWidth = 84, valueHeight = 18;
constexpr int columnGap = (card::width - 2 * card::padding - leftColumn - valuesWidth - rightColumn) / 2;
constexpr int shapeWidth = 26, shapeHeight = 20;
// The strip: each Shape 32 x 28 px, 1 px apart, inside a 1 px border and 3 px of padding.
constexpr int optionWidth = 32, optionHeight = 28, optionGap = 1, stripInset = 4, stripGap = 6;
} // namespace

//==============================================================================
class HoverCard::ShapeChoice final : public juce::Button
{
public:
    explicit ShapeChoice (const juce::String& name) : juce::Button (name)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    Shape shown = Shape::Bell;
    juce::Colour iconColour = colour::text2;
    bool marked = false; // the strip's Shape now, or the card's Shape while its strip is open

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto area = getLocalBounds().toFloat();
        if (marked || highlighted)
        {
            g.setColour (marked ? colour::fill2 : colour::fill1);
            g.fillRoundedRectangle (area, tokens::size::r1);
        }
        staple::drawIcon (g, shapeIcon (shown), juce::Rectangle<float> (20.0f, 12.0f).withCentre (area.getCentre()), iconColour);
    }
};

//==============================================================================
HoverCard::Value::Value (Kind k) : kind (k)
{
    setSliderStyle (juce::Slider::LinearBarVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    const char* names[] = { "Frequency", "Gain", "Q" };
    setTooltip (juce::String (names[static_cast<int> (kind)]) + juce::String::fromUTF8 (" \xc2\xb7 drag up/down \xc2\xb7 double-click to type"));
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

HoverCard::Value::~Value() { stop(); }

juce::String HoverCard::Value::text()
{
    if (! isEnabled() && readOnlyText.isNotEmpty())
        return readOnlyText;
    const double value = getValue();
    switch (kind)
    {
        case Kind::frequency:
            if (value >= 1000.0)
                return juce::String (value / 1000.0, 2) + " kHz";
            return juce::String (value, value < 100.0 ? 1 : 0) + " Hz";
        case Kind::gain:
            return (value > 0.0 ? "+" : "") + juce::String (value, 2) + " dB";
        case Kind::q:
            return "Q " + juce::String (value, value < 1.0 ? 3 : 2);
    }
    return {};
}

void HoverCard::Value::stop()
{
    if (isMouseDragging())
        endMouseDrag();
    closeField (false);
}

void HoverCard::Value::paint (juce::Graphics& g)
{
    if (isTypingIn())
        return;
    const auto area = getLocalBounds().toFloat();
    if (isMouseDragging())
    {
        g.setColour (colour::fill2);
        g.fillRoundedRectangle (area, tokens::size::r1);
    }
    g.setFont (staple::font (tokens::size::fs3, kind == Kind::frequency ? staple::Weight::semiBold : staple::Weight::regular));
    g.setColour (! isEnabled() ? colour::text3 : kind == Kind::q ? colour::text2 : colour::text1);
    g.drawText (text(), area.withTrimmedLeft (4.0f), juce::Justification::centredLeft, false);
}

void HoverCard::Value::resized()
{
    if (field != nullptr)
        field->setBounds (getLocalBounds());
}

void HoverCard::Value::mouseDown (const juce::MouseEvent& e)
{
    if (isEnabled() && ! isTypingIn())
        startMouseDrag (e);
    repaint();
}

void HoverCard::Value::mouseDrag (const juce::MouseEvent& e)
{
    if (isMouseDragging())
        continueMouseDrag (e);
}

void HoverCard::Value::mouseUp (const juce::MouseEvent&)
{
    if (isMouseDragging())
        endMouseDrag();
    repaint();
}

void HoverCard::Value::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled() || isTypingIn())
        return;
    if (isMouseDragging())
        endMouseDrag();
    field = staple::makeTypeInField (getTitle() + " value", textToType (getTextFromValue (getValue())), [this] { closeField (true); },
                                     [this] { closeField (false); });
    addAndMakeVisible (*field);
    resized();
    repaint();
    staple::focusTypeInField (*field);
}

void HoverCard::Value::closeField (bool commit)
{
    if (field == nullptr)
        return;
    const auto typed = staple::closeTypeInField (field);
    if (commit)
        commitTypedText (typed);
    repaint();
}

void HoverCard::Value::enablementChanged()
{
    setMouseCursor (isEnabled() ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

double HoverCard::Value::valueDraggedBy (double from, float pixels, bool fine) { return staple::draggedAlongRange (*this, from, pixels, fine); }

//==============================================================================
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

    shape = std::make_unique<ShapeChoice> ("Hover Card Shape");
    shape->onClick = [this] {
        if (shapeStrip() != nullptr)
            closeStrip();
        else
            openStrip();
    };
    addAndMakeVisible (*shape);

    frequency = std::make_unique<Value> (Value::Kind::frequency);
    gain = std::make_unique<Value> (Value::Kind::gain);
    q = std::make_unique<Value> (Value::Kind::q);
    for (auto* value : values())
    {
        value->onValueChange = [value] { value->repaint(); };
        addAndMakeVisible (*value);
    }
    fade.apply = [this] (float alpha) {
        for (auto* value : values())
            value->setAlpha (alpha);
    };

    // Its controls report entering and leaving it.
    addMouseListener (this, true);
}

HoverCard::~HoverCard()
{
    removeMouseListener (this);
    if (strip != nullptr)
        strip->removeMouseListener (this);
    for (auto* value : values())
        value->stop();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
}

bool HoverCard::isFrozen() const
{
    const auto all = values();
    return std::any_of (all.begin(), all.end(), [] (const Value* value) { return value->isBusy(); });
}

bool HoverCard::isHeld() const { return menuOpen || shapeStrip() != nullptr || isFrozen(); }

juce::Colour HoverCard::bandColour() const { return tokens::band[static_cast<size_t> (std::max (1, slot) - 1)]; }

void HoverCard::openStrip()
{
    auto* layer = getParentComponent();
    if (slot == 0 || layer == nullptr)
        return;
    if (strip == nullptr)
    {
        struct Strip final : juce::Component
        {
            juce::OwnedArray<ShapeChoice> options;
            void paint (juce::Graphics& g) override
            {
                const auto area = getLocalBounds().toFloat();
                g.setColour (colour::menu);
                g.fillRoundedRectangle (area, tokens::size::r3);
                g.setColour (colour::line2);
                g.drawRoundedRectangle (area.reduced (0.5f), tokens::size::r3, 1.0f);
            }
        };
        auto made = std::make_unique<Strip>();
        made->setTitle ("Shape");
        made->setWantsKeyboardFocus (false);
        for (int i = 0; i < parameters::shapeNames().size(); ++i)
        {
            auto option = std::make_unique<ShapeChoice> (parameters::shapeNames()[i]);
            option->setTitle (parameters::shapeNames()[i]);
            option->shown = static_cast<Shape> (i);
            option->setBounds (stripInset + i * (optionWidth + optionGap), stripInset, optionWidth, optionHeight);
            option->onClick = [this, picked = option->shown] {
                if (slot != 0)
                    editing.setShape (slot, picked);
                closeStrip();
                refresh();
            };
            made->addAndMakeVisible (*made->options.add (option.release()));
        }
        strip = std::move (made);
        strip->setSize (2 * stripInset + parameters::shapeNames().size() * (optionWidth + optionGap) - optionGap, 2 * stripInset + optionHeight);
        strip->addMouseListener (this, true);
    }
    layer->addAndMakeVisible (*strip);
    strip->toFront (false);
    const auto current = editing.band (slot).shape;
    for (auto* child : strip->getChildren())
        if (auto* option = dynamic_cast<ShapeChoice*> (child))
        {
            option->marked = option->shown == current;
            option->iconColour = option->marked ? bandColour() : colour::text2;
            option->repaint();
        }
    placeStrip();
    shape->marked = true;
    shape->repaint();
}

void HoverCard::closeStrip()
{
    if (strip == nullptr)
        return;
    strip->setVisible (false);
    shape->marked = false;
    shape->repaint();
}

void HoverCard::placeStrip()
{
    if (shapeStrip() == nullptr)
        return;
    const auto cardArea = body();
    const int width = strip->getWidth(), height = strip->getHeight();
    const int x = juce::jlimit (within.getX() + card::inset, std::max (within.getX() + card::inset, within.getRight() - card::inset - width), cardArea.getX());
    const bool room = cardArea.getBottom() + stripGap + height <= within.getBottom() - card::inset;
    strip->setTopLeftPosition (x, room ? cardArea.getBottom() + stripGap : cardArea.getY() - stripGap - height);
}

void HoverCard::show (int newSlot, juce::Point<float> handle, juce::Rectangle<int> display)
{
    within = display;
    if (newSlot != slot)
        attach (newSlot);
    else if (isFrozen())
    {
        // It stays where it opened while a value is dragged or typed in.
        refresh();
        return;
    }
    const auto x = juce::roundToInt (handle.x), y = juce::roundToInt (handle.y);
    above = y - within.getY() > card::roomAbove;
    auto area = juce::Rectangle<int> (card::width, card::height).withCentre ({ x, 0 });
    area.setY (above ? y - card::gap - card::height : y + card::gap);
    area.setX (juce::jlimit (within.getX() + card::inset, std::max (within.getX() + card::inset, within.getRight() - card::inset - card::width),
                             area.getX()));
    const float newTipX = handle.x - static_cast<float> (area.getX() - margin);
    setBounds (area.expanded (margin));
    placeStrip();
    if (! juce::approximatelyEqual (newTipX, tipX))
    {
        tipX = newTipX;
        repaint();
    }
    refresh();
    setVisible (true);
}

void HoverCard::hide()
{
    attach (0);
    closeStrip();
    pointerOver = false;
    setVisible (false);
}

void HoverCard::attach (int newSlot)
{
    for (auto* value : values())
        value->stop();
    // The old attachments go first, so they let go of the values.
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    slot = newSlot;
    if (slot == 0)
        return;

    auto& state = processor.parameterState();
    frequencyAttachment = std::make_unique<SliderAttachment> (state, parameters::frequencyId (slot), *frequency);
    gainAttachment = std::make_unique<SliderAttachment> (state, parameters::gainId (slot), *gain);
    qAttachment = std::make_unique<SliderAttachment> (state, parameters::qId (slot), *q);
    const std::pair<Value*, juce::String> attached[] = { { frequency.get(), parameters::frequencyId (slot) },
                                                         { gain.get(), parameters::gainId (slot) },
                                                         { q.get(), parameters::qId (slot) } };
    for (const auto& [value, id] : attached)
    {
        // No reset gesture: a double-click types a value.
        value->setDoubleClickReturnValue (false, 0.0);
        value->describe (*state.getParameter (id));
    }

    const auto band = "Band " + juce::String (slot);
    setTitle (band + " quick controls");
    bypass.setTitle (band + " Bypass");
    deleteButton.setTitle ("Delete " + band);
    more.setTitle (band + " menu");
    shape->setTitle (band + " Shape");
    closeStrip();
    fade.jump (editing.band (slot).bypass ? card::bypassedAlpha : 1.0f);
    repaint();
}

void HoverCard::refresh()
{
    if (slot == 0)
        return;
    const auto band = editing.band (slot);
    bypass.setToggleState (band.bypass, juce::dontSendNotification);
    if (shape->shown != band.shape || shape->iconColour != bandColour())
    {
        shape->shown = band.shape;
        shape->iconColour = bandColour();
        shape->repaint();
    }
    // Gain's place shows a Cut's Slope, or that the Shape has no Gain.
    gain->setEnabled (hasGain (band.shape));
    gain->readOnlyText = isCut (band.shape) ? slopeText (band.slope, band.brickwall) : "No Gain";
    // Flat Tilt's design ignores Q.
    q->setEnabled (band.shape != Shape::FlatTilt);
    fade.towards (band.bypass ? card::bypassedAlpha : 1.0f);
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
    shape->setBounds (left.removeFromBottom (shapeHeight).withSizeKeepingCentre (shapeWidth, shapeHeight));
    deleteButton.setBounds (right.removeFromTop (button));
    more.setBounds (right.removeFromBottom (button));
    auto middle = area.withTrimmedLeft (columnGap).withWidth (valuesWidth);
    for (auto* value : values())
        value->setBounds (middle.removeFromTop (valueHeight));
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
