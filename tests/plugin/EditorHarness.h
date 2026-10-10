#pragma once

#include "EqDisplay.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace harness
{

// The first child of parent, at any depth, that is a T and passes test.
template <typename T>
T* findChild (juce::Component& parent, std::function<bool (T&)> test = [] (T&) { return true; })
{
    for (auto* child : parent.getChildren())
    {
        if (auto* found = dynamic_cast<T*> (child); found != nullptr && test (*found))
            return found;
        if (auto* found = findChild<T> (*child, test))
            return found;
    }
    return nullptr;
}

// The editor in a window of its own, as a host shows it, so it takes keyboard focus. Keys go through
// the window, to the focused control and up through its parents, as the user's do; the mouse goes
// straight to the EQ display.
struct OpenEditor
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() };
    eq1::EqDisplay& display = *findChild<eq1::EqDisplay> (*editor);

    OpenEditor()
    {
        editor->addToDesktop (0);
        editor->setVisible (true);
    }

    ~OpenEditor()
    {
        juce::PopupMenu::dismissAllActiveMenus();
        editor.reset();
    }

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }
    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }
    float value (int slot, const char* control) { return value ("band" + juce::String (slot) + "_" + control); }

    void set (const juce::String& id, float plain) { parameter (id).setValueNotifyingHost (parameter (id).convertTo0to1 (plain)); }
    void set (int slot, const char* control, float plain) { set ("band" + juce::String (slot) + "_" + control, plain); }

    void addBand (int slot, float frequency, float gain, float shape = 0.0f)
    {
        set (slot, "shape", shape);
        set (slot, "frequency", frequency);
        set (slot, "gain", gain);
        set (slot, "in_use", 1.0f);
    }

    // Lets the editor's timers run for a while, so it shows what the parameters hold.
    void settle (int milliseconds = 60)
    {
        const auto end = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (milliseconds);
        while (juce::Time::getMillisecondCounter() < end)
        {
            juce::Timer::callPendingTimersSynchronously();
            juce::Thread::sleep (5);
        }
    }

    // Where the display draws a Band at frequency with Gain 0: on a log scale from 10 Hz to 30 kHz,
    // halfway down.
    juce::Point<float> at (double frequency) const
    {
        return { static_cast<float> (std::log (frequency / 10.0) / std::log (3000.0) * display.getWidth()),
                 static_cast<float> (display.getHeight()) * 0.5f };
    }

    juce::MouseEvent mouseEvent (juce::Point<float> position, juce::ModifierKeys mods, juce::Point<float> downAt)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(),
                 position,
                 mods,
                 juce::MouseInputSource::defaultPressure,
                 juce::MouseInputSource::defaultOrientation,
                 juce::MouseInputSource::defaultRotation,
                 juce::MouseInputSource::defaultTiltX,
                 juce::MouseInputSource::defaultTiltY,
                 &display,
                 &display,
                 now,
                 downAt,
                 now,
                 1,
                 position != downAt };
    }

    // A press at from, a drag to to and a release.
    void drag (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods)
    {
        display.mouseDown (mouseEvent (from, mods, from));
        display.mouseDrag (mouseEvent (to, mods, from));
        display.mouseUp (mouseEvent (to, mods.withoutMouseButtons(), from));
    }

    // A left-click on the display, as on a Band's handle to select it.
    void click (juce::Point<float> position)
    {
        const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
        display.mouseDown (mouseEvent (position, left, position));
        display.mouseUp (mouseEvent (position, {}, position));
    }

    // Every child of the editor, at any depth, that is a T and passes test.
    template <typename T>
    std::vector<T*> findAll (std::function<bool (T&)> test = [] (T&) { return true; })
    {
        std::vector<T*> found;
        std::function<void (juce::Component&)> visit = [&] (juce::Component& parent) {
            for (auto* child : parent.getChildren())
            {
                if (auto* t = dynamic_cast<T*> (child); t != nullptr && test (*t))
                    found.push_back (t);
                visit (*child);
            }
        };
        visit (*editor);
        return found;
    }

    // A key pressed and released in the window, as the user types it.
    bool press (const juce::KeyPress& key)
    {
        const bool used = hold (key);
        release();
        return used;
    }
    // A key pressed, or auto-repeated while held, without its release.
    bool hold (const juce::KeyPress& key)
    {
        editor->getPeer()->handleKeyUpOrDown (true);
        return editor->getPeer()->handleKeyPress (key);
    }
    void release() { editor->getPeer()->handleKeyUpOrDown (false); }
};

} // namespace harness
