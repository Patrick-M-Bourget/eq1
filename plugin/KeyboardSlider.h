#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace eq1
{

// A Slider that keyboard focus reaches by Tab, once (not again in its text box), though a click
// leaves focus where it was. The arrow keys step it 1% of its normalised range, 0.2% with Shift (so
// Frequency and Q step evenly in log), within its range and at least one interval, unless it is given
// steps in its own units (setArrowSteps: Gain Scale's 5%, 1% with Shift). A key held, with
// its repeats, is one drag: one gesture on a host parameter, so one undo step, ended by the key's
// release or by losing focus.
//
// A screen reader reads it as one element: its title and its value with the unit (its text box isn't
// another).
class KeyboardSlider : public juce::Slider
{
public:
    explicit KeyboardSlider (const juce::String& name = {});
    ~KeyboardSlider() override;

    // Where a step from from towards to lands, for a slider whose positions aren't all values;
    // by default, to.
    std::function<double (double from, double to)> landStep;

    // The arrow keys step it by step, or fineStep with Shift, in its own units, rather than by a
    // proportion of its range.
    void setArrowSteps (double step, double fineStep);

    // What a screen reader reads as its value; by default, its text.
    std::function<juce::String (double value)> spokenValue;
    // Titled with the parameter's name, and read with its value and unit (accessibility::spokenValue).
    void describe (const juce::RangedAudioParameter& parameter);

    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType cause) override;

protected:
    // A mouse drag along one axis, from where the press left it: the press, its drag and its release are
    // one gesture (one undo step), and the value moves by valueDraggedBy from where the drag is anchored.
    // Shift pressed or let go mid-drag re-anchors where the value is. The second press of a double-click
    // sets the double-click return value (juce::Slider's setDoubleClickReturnValue) inside that gesture
    // and anchors there, so a drag carries on from it; the double-click JUCE sends after the release
    // has nothing more to do.
    enum class DragAxis
    {
        vertical,  // pixels up
        horizontal // pixels right
    };
    // A press that sets a value itself (a click on a track) passes it as pressed: the drag anchors there.
    void startMouseDrag (const juce::MouseEvent& e, DragAxis axis = DragAxis::vertical, std::optional<double> pressed = {});
    void continueMouseDrag (const juce::MouseEvent& e);
    void endMouseDrag();
    bool isMouseDragging() const { return mouseDrag != nullptr; }
    // Where a drag of pixels from the anchor's value, from, lands (fine with Shift); by default, from.
    virtual double valueDraggedBy (double from, float pixels, bool fine);

private:
    struct MouseDrag
    {
        explicit MouseDrag (KeyboardSlider& slider) : gesture (slider) {}
        DragAxis axis = DragAxis::vertical;
        float anchor = 0.0f;
        double anchorValue = 0.0;
        bool fine = false;
        ScopedDragNotification gesture;
    };
    std::unique_ptr<MouseDrag> mouseDrag;
    float along (const juce::MouseEvent& e) const;

    void endHeldStep();
    void childrenChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::optional<ScopedDragNotification> held; // while an arrow key is held
    std::optional<std::pair<double, double>> arrowSteps;
};

} // namespace eq1
