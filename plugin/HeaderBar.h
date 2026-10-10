#pragma once

#include "ABCompare.h"
#include "PresetBar.h"
#include "staple/Wordmark.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/TextChip.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

class PluginProcessor;

// The A/B Compare button (HANDOFF.md §5.6): one plain text button reading "A/B", the side you're on
// in text1 and the other in text4, the slash in text4 at 400. The letters' colours change over dur2.
// A screen reader reads the side you're on as its value.
class CompareButton final : public staple::TextChip, private juce::Timer
{
public:
    CompareButton();

    void showSide (CompareSide side);
    // A letter's colour now, part way through a change over dur2.
    juce::Colour letterInk (CompareSide letter) const;
    int getIdealWidth() const;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    void timerCallback() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    float progress() const;

    CompareSide side = CompareSide::A, previousSide = CompareSide::A;
    double changedAt = 0.0;
};

// The window's header (HANDOFF.md §4, §5.6, §9B): the wordmark on the left; ‹, the Presets button and
// › in the centre (PresetBar); Undo, Redo, A/B Compare and Copy on the right. The side groups share
// the leftover width equally, so the Preset group stays centred in the window. Copy copies the side
// you're on to the other side, its tooltip naming the direction, and reads "Copied" for 1 s.
class HeaderBar final : public juce::Component, private juce::MultiTimer
{
public:
    explicit HeaderBar (PluginProcessor& processor);

    void paint (juce::Graphics& g) override;
    void resized() override;

    PresetBar& presets() { return presetBar; }
    juce::Rectangle<int> wordmarkArea() const { return wordmark; }

    void undo();
    void redo();
    // Enables Undo and Redo by what can be undone and redone.
    void showUndoState();

private:
    enum TimerId
    {
        followTimer,
        copiedTimer
    };
    void timerCallback (int id) override;
    // Follows the side, which edits, undo and restoring a session change too.
    void showSide();
    void copyToOther();
    void edited();
    int rightGroupWidth() const;

    PluginProcessor& processor;
    PresetBar presetBar;
    staple::IconButton undoButton { "Undo", staple::Icon::undo }, redoButton { "Redo", staple::Icon::redo };
    CompareButton compareButton;
    staple::TextChip copyButton { "Copy", staple::TextChip::Look::plain, staple::tokens::size::fs4 };
    int copyWidth = 0;
    juce::Rectangle<int> wordmark;
};

} // namespace eq1
