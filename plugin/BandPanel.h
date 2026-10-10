#pragma once

#include "DynamicRangeRing.h"
#include "DynamicsSection.h"
#include "KeyboardSlider.h"
#include "staple/controls/EdgeSelector.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Knob.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace eq1
{

class PluginProcessor;
class BandEditing;

// The selected Band's settings, on Staple's floating slab with a bell rising from its top edge
// (HANDOFF.md §4 "Band panel"). The top row has Bypass, Solo (held: the Band is Soloed while the button
// is down), the ‹ n › Band selector, which steps through the Bands in use in Frequency order, and Delete.
// Shape and Stereo Placement are Edge selectors flush with its sides, Slope a text button under Shape
// that opens its list on a click, Space or Return, drags continuously and takes a typed value from the list,
// and Frequency, Gain and Q knobs sit between them, each attached to its host parameter. Controls a
// Band's Shape or the track doesn't offer dim and are disabled, so Tab skips them: Gain on Shapes without
// Gain, Slope on Shapes without one, Q on Flat Tilt and Stereo Placement on mono. A Bypassed Band's
// panel fades to 38 % but Bypass and Delete, and stays editable. The panel is hidden while no Band is
// selected.
//
// Dynamics (HANDOFF.md §4, §5.3): Dynamic Range is a ring round the Gain knob (DynamicRangeRing). On a
// Dynamic Band, Clear Dynamics, Dynamics Bypass and » show above Gain, and » opens and closes the
// dynamics section between Gain and Q (DynamicsSection) with a slide, the panel widening by the
// section's width about its centre; nothing else changes its width. The section is open unless closed
// by », for as long as the editor is open, and absent on a Band that isn't dynamic. The Band shown is
// the Metered Band while its Shape has dynamics.
class BandPanel final : public juce::Component, private juce::Timer
{
public:
    // The panel's size at 100 % UI Scale: the slab and the bell above it; wider by the section while
    // the dynamics section is open.
    static constexpr int width = 492, height = 137;
    static constexpr int openWidth = width + staple::tokens::layout::dynamicsSectionWidth + 8;

    BandPanel (PluginProcessor& processor, BandEditing& editing);
    ~BandPanel() override;

    // The Band Slot to show, or 0 for none.
    void show (int slot);
    int shownSlot() const { return slot; }

    // Where the panel's bottom centre sits in its parent; it keeps it as its width changes.
    void setAnchor (juce::Point<int> bottomCentre);
    // The dynamics section is open, or opening.
    bool isDynamicsOpen() const;

    // Called with the Band to select when ‹ or › is pressed; the editor selects it on the display, which
    // shows it here. Without it, the panel shows that Band itself.
    std::function<void (int)> onSelectBand;

    // The Slope: its value as "12 dB/oct", or "Brickwall" on a Brickwall Cut. A click, Space or Return
    // lists the Slopes and "Type a value…"; a vertical drag changes it over 0 to 96 dB/oct (200 px, 800
    // with Shift) and a typed value sets it, each one undo step that also turns a Cut's Brickwall off.
    class SlopeButton;

    void paint (juce::Graphics& g) override;
    void resized() override;
    // On the slab and its bell only, so clicks beside the bell reach the display.
    bool hitTest (int x, int y) override;

private:
    void timerCallback() override;
    // Which controls are available, the Band's colour, and the Bypassed fade.
    void updateAvailability();
    // Each control's accessible title, from its parameter's name ("Band 4 Gain"), and its spoken value.
    void describe();
    void releaseSolo();
    // The previous (-1) or next (1) Band in use by Frequency, wrapping.
    void step (int direction);
    // The dynamics icons and section for the Band as it is now, sliding the section if animate.
    void showDynamics (bool animate);
    void placeAtWidth();
    // The controls that fade while the Band is Bypassed: all but Bypass and Delete.
    std::vector<juce::Component*> faded();
    juce::Colour bandColour() const;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    BandEditing& editing;
    int slot = 0;

    staple::IconButton bypass { "Bypass", staple::Icon::power }, solo { "Solo", staple::Icon::headphones },
        previous { "Previous Band", staple::Icon::previous }, next { "Next Band", staple::Icon::next },
        deleteButton { "Delete", staple::Icon::close };
    staple::EdgeSelector shape { "Shape", staple::EdgeSelector::Side::left }, placement { "Stereo Placement", staple::EdgeSelector::Side::right };
    std::unique_ptr<SlopeButton> slope;
    staple::Knob frequency { staple::tokens::knob::frequency, "Frequency" }, gain { staple::tokens::knob::gain, "Gain" },
        q { staple::tokens::knob::q, "Q" };
    juce::Label frequencyLabel, gainLabel, qLabel;

    DynamicRangeRing ring;
    // Above Gain on a Dynamic Band: Clear Dynamics, Dynamics Bypass and the section's chevron.
    juce::Component dynamicsIcons;
    staple::IconButton clearDynamics { "Clear Dynamics", staple::Icon::close }, dynamicsBypass { "Dynamics Bypass", staple::Icon::power },
        dynamicsOpen { "Dynamics", staple::Icon::dynamicsOpen };
    DynamicsSection section;
    bool sectionWanted = true; // open unless closed by », while the editor is open
    std::optional<juce::Point<int>> anchor;

    // A value eased from one target to the next over durationMs: the Bypassed fade, the section's slide.
    struct Tween final : juce::Timer
    {
        explicit Tween (int ms, float start) : durationMs (ms), from (start), to (start), now (start) {}
        std::function<void (float)> apply;
        int durationMs;
        float from, to, now;
        double startedMs = 0.0;
        void towards (float target);
        void jump (float target);
        void timerCallback() override;
    };
    Tween fade { staple::tokens::motion::dur2Ms, 1.0f }, slide { staple::tokens::motion::dur3Ms, 1.0f };

    juce::Path slab; // the body and its bell, in the panel's coordinates
    std::array<int, 2> dividers {}; // their x
    juce::Rectangle<int> numberArea;

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, placementAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment, slopeAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, dynamicsBypassAttachment;
};

class BandPanel::SlopeButton final : public staple::Knob
{
public:
    explicit SlopeButton (BandPanel& panel);

    // What it reads: "12 dB/oct", "37.5 dB/oct", or "Brickwall".
    juce::String text();
    // The Slope list (BandMenu.h's slopeMenu), then "Type a value…", which opens the tooltip's type-in.
    juce::PopupMenu list();
    // The list, as a click, Space or Return opens it.
    void openList();

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;
    void valueChanged() override;

private:
    // Any edit of the Slope here, a drag, a typed value or a key, is a Slope in dB/oct: it turns a Cut's
    // Brickwall off, in the same undo step (overlapping gestures are one).
    void startedDragging() override;
    void stoppedDragging() override;

    static constexpr int dragThreshold = 3;
    BandPanel& panel;
    bool dragging = false, fine = false;
    float pressY = 0.0f;
    double startProportion = 0.0;
    std::optional<ScopedDragNotification> gesture;
    juce::RangedAudioParameter* brickwall = nullptr;
};

} // namespace eq1
