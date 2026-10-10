#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

namespace eq1
{

// A Slider that keyboard focus reaches by Tab, once (not again in its text box), though a click
// leaves focus where it was. The arrow keys step it 1% of its normalised range, 0.2% with Shift (so
// Frequency and Q step evenly in log), within its range and at least one interval. A key held, with
// its repeats, is one drag: one gesture on a host parameter, so one undo step, ended by the key's
// release or by losing focus.
class KeyboardSlider : public juce::Slider
{
public:
    explicit KeyboardSlider (const juce::String& name = {});
    ~KeyboardSlider() override;

    // Where a step from from towards to lands, for a slider whose positions aren't all values;
    // by default, to.
    std::function<double (double from, double to)> landStep;

    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType cause) override;

private:
    void endHeldStep();

    std::optional<ScopedDragNotification> held; // while an arrow key is held
};

} // namespace eq1
