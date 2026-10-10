#pragma once

#include "KeyboardSlider.h"
#include "staple/Tokens.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Tween.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// The Hover Card (HANDOFF.md §2 "HoverCard", §5.2): quick controls for the Band whose handle the
// pointer rests on, which edit that Band alone and never select it. Left, Bypass and the Shape; in the
// middle Frequency, Gain and Q; right, Delete and ▾, which opens the Band menu (BandMenu.h) for that
// one Band. Every edit is one undo step. The EQ display shows, places and hides it (EqDisplay.h). It is
// for the mouse only: neither it nor its controls take keyboard focus, and the Band panel serves the
// keyboard. A screen reader reads it as a group, "Band 4 quick controls".
class HoverCard final : public juce::Component
{
public:
    HoverCard (PluginProcessor& processor, BandEditing& editing);
    ~HoverCard() override;

    // The Band it shows, or 0 while hidden.
    int shownSlot() const { return slot; }

    // Shows the card for a Band whose handle is at handle, or moves it there: centred 18 px above it, or
    // below it when the handle is near the top of within (the display), and kept 6 px inside its left and
    // right; both in the card's parent's coordinates. It reads the Band afresh each time. Hides it.
    void show (int slot, juce::Point<float> handle, juce::Rectangle<int> within);
    void hide();

    // The card itself, in its parent's coordinates: the component is wider, for its shadow and arrow tip.
    juce::Rectangle<int> body() const { return bodyArea() + getPosition(); }

    // The pointer is over the card.
    bool isPointerOver() const { return pointerOver; }
    // It stays up, whatever the pointer does, while its menu is open or a value is dragged or typed in;
    // it stays where it is, too, while a value is dragged or typed in.
    bool isHeld() const;

    // Frequency, Gain or Q, as text attached to its parameter: a vertical drag moves it as a knob does
    // (staple::draggedAlongRange), one undo step, and a double-click opens a field to type it in
    // (staple/controls/TypeIn.h: Enter sets it as one undo step, Esc or a click away cancels). There is
    // no reset gesture. Disabled, it shows readOnlyText dimmed.
    class Value;

    // What ▾ opens: the Band menu for the shown Band alone. Its Delete deletes that Band, and its Split
    // and Paste leave the selection as it is.
    juce::PopupMenu menu();
    // The Band menu's Select All.
    std::function<void()> onSelectAll;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    // Attaches the values to a Band's parameters (0: none), ending any drag or type-in, and names the
    // card and its controls for it.
    void attach (int slot);
    // The controls' states for the shown Band, as it is now.
    void refresh();
    void openMenu();
    bool isFrozen() const;

    // Room around the body for its shadow and arrow tip, which the pointer passes through.
    static constexpr int margin = 32;
    juce::Rectangle<int> bodyArea() const { return getLocalBounds().reduced (margin); }

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;
    bool pointerOver = false, menuOpen = false;
    bool above = true; // the card is above its handle, its tip pointing down
    float tipX = 0.0f; // the handle's x, in the card's coordinates

    // Named apart from the Band panel's buttons; titled for the shown Band by refresh().
    staple::IconButton bypass { "Hover Card Bypass", staple::Icon::power }, deleteButton { "Hover Card Delete", staple::Icon::close },
        more { "Hover Card Menu", staple::Icon::dropdown };
    std::unique_ptr<Value> frequency, gain, q;
    std::array<Value*, 3> values() const { return { frequency.get(), gain.get(), q.get() }; }
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment;
    // The values' Bypassed fade.
    staple::Tween fade { staple::tokens::motion::dur2Ms, 1.0f };
};

class HoverCard::Value final : public KeyboardSlider
{
public:
    enum class Kind
    {
        frequency,
        gain,
        q
    };
    explicit Value (Kind kind);
    ~Value() override;

    // What it shows: "1.00 kHz" or "85.0 Hz", "+3.00 dB", "Q 0.707"; readOnlyText instead while disabled,
    // unless empty.
    juce::String text();
    juce::String readOnlyText;

    bool isTypingIn() const { return field != nullptr; }
    juce::TextEditor* getTypeInField() const { return field.get(); }
    bool isBusy() const { return isMouseDragging() || isTypingIn(); }
    // Ends a drag or a type-in at once, the typed text unused: its Band went.
    void stop();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void enablementChanged() override;

private:
    double valueDraggedBy (double from, float pixels, bool fine) override;
    void closeField (bool commit);

    const Kind kind;
    std::unique_ptr<juce::TextEditor> field;
};

} // namespace eq1
