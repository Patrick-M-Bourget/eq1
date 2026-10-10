#pragma once

#include "DetectionArc.h"
#include "KeyboardSlider.h"
#include "staple/controls/EdgeSelector.h"
#include "staple/controls/IconButton.h"
#include "staple/controls/Knob.h"
#include "staple/controls/TextChip.h"

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
// that lists the Slopes (BandMenu.h's slopeMenu) on a click, drags continuously and takes a typed value,
// and Frequency, Gain and Q knobs sit between them, each attached to its host parameter. Controls a
// Band's Shape or the track doesn't offer dim and are disabled, so Tab skips them: Gain on Shapes without
// Gain, Slope on Shapes without one, Q on Flat Tilt and Stereo Placement on mono. A Bypassed Band's
// panel fades to 38 % but Bypass and Delete, and stays editable. The panel keeps one width whatever it
// shows, and is hidden while no Band is selected.
//
// The dynamics (Dynamic Range, Threshold with its Detection Level arc, Attack, Release, Dynamics Bypass,
// Detection Source, Detection Range and its Free limits, Detection Audition) open in a call-out from a
// Dynamics button above Gain, on Shapes with dynamics. Threshold's top position is Auto, its own host
// parameter (ADR 0003). Holding Detection Audition plays the Band's detection signal until it is
// released. The Band shown is the Metered Band while its Shape has dynamics.
class BandPanel final : public juce::Component, private juce::Timer
{
public:
    // The panel's size at 100 % UI Scale: the slab and the bell above it.
    static constexpr int width = 492, height = 137;

    BandPanel (PluginProcessor& processor, BandEditing& editing);
    ~BandPanel() override;

    // The Band Slot to show, or 0 for none.
    void show (int slot);
    int shownSlot() const { return slot; }

    // Called with the Band to select when ‹ or › is pressed; the editor selects it on the display, which
    // shows it here. Without it, the panel shows that Band itself.
    std::function<void (int)> onSelectBand;

    // The Slope: its value as "12 dB/oct", or "Brickwall" on a Brickwall Cut. A click lists the Slopes;
    // a vertical drag changes it over 0 to 96 dB/oct (200 px, 800 with Shift) and a double-click types a
    // value, each one undo step that also turns a Cut's Brickwall off.
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
    // Threshold and Auto Threshold, two host parameters, on one slider.
    void showThreshold();
    void storeThreshold();
    void releaseAudition();
    void releaseSolo();
    // The previous (-1) or next (1) Band in use by Frequency, wrapping.
    void step (int direction);
    void openDynamics();
    // The controls that fade while the Band is Bypassed: all but Bypass and Delete.
    std::vector<juce::Component*> faded();
    void setFade (float alpha);
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
    staple::TextChip dynamicsButton { "Dynamics", staple::TextChip::Look::filled, staple::tokens::size::fs2 };
    bool soloHeld = false;

    // Until the dynamics section (#82): today's dynamics controls, lent to a call-out while it is open.
    struct Dynamics final : juce::Component
    {
        Dynamics (PluginProcessor& processor);
        void resized() override;
        // The rotary controls, left to right, with their labels.
        std::array<std::pair<juce::Slider*, juce::Label*>, 6> rotaries();

        juce::ComboBox detectionSource, detectionRange;
        KeyboardSlider dynamicRange, threshold, attack, release, detectionLow, detectionHigh;
        juce::Label dynamicRangeLabel, thresholdLabel, attackLabel, releaseLabel, detectionLowLabel, detectionHighLabel;
        DetectionArc detectionArc;
        juce::ToggleButton dynamicsBypass { "Dynamics Bypass" };
        juce::TextButton audition { "Detection Audition" };
    } dynamics;
    juce::Component::SafePointer<juce::CallOutBox> dynamicsCallOut;

    // The Bypassed fade, eased over dur2.
    struct Fade final : juce::Timer
    {
        std::function<void (float)> apply;
        float from = 1.0f, to = 1.0f, now = 1.0f;
        double startedMs = 0.0;
        void towards (float target);
        void timerCallback() override;
    } fade;

    juce::Path slab; // the body and its bell, in the panel's coordinates
    std::array<int, 2> dividers {}; // their x
    juce::Rectangle<int> numberArea;

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, placementAttachment, detectionSourceAttachment, detectionRangeAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment, gainAttachment, qAttachment, slopeAttachment, dynamicRangeAttachment,
        attackAttachment, releaseAttachment, detectionLowAttachment, detectionHighAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, dynamicsBypassAttachment;
    std::unique_ptr<juce::ParameterAttachment> thresholdAttachment, thresholdAutoAttachment;
    bool thresholdDragging = false;
};

class BandPanel::SlopeButton final : public staple::Knob
{
public:
    explicit SlopeButton (BandPanel& panel);

    // What it reads: "12 dB/oct", "37.5 dB/oct", or "Brickwall".
    juce::String text();
    // The Slope list, as a click opens it.
    void openList();

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
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
    int clicks = 0; // a click's list opens only if no other click came within the double-click time
    std::optional<ScopedDragNotification> gesture;
    juce::RangedAudioParameter* brickwall = nullptr;
};

} // namespace eq1
