#pragma once

#include "EqDisplay.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
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

// The folder EQ1_SCREENS names, where renders for checking by hand against the prototype go
// (scripts/check.sh screens <dir> runs every hidden [.screens] test with it); empty when unset. The one
// place a test reads an EQ1_ environment variable.
inline juce::String snapshotFolder() { return juce::SystemStats::getEnvironmentVariable ("EQ1_SCREENS", {}); }

// Writes image as <name>.png into the snapshot folder, if one is set.
inline void writeSnapshot (const juce::Image& image, const juce::String& name)
{
    const auto folder = snapshotFolder();
    if (folder.isEmpty())
        return;
    REQUIRE (image.isValid());
    const auto file = juce::File (folder).getChildFile (name + ".png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    CHECK (juce::PNGImageFormat().writeImageToStream (image, stream));
}

// Renders area of component (all of it when empty) at scale and writes it as name (above); renders
// nothing when no snapshot folder is set.
inline void writeSnapshot (juce::Component& component, const juce::String& name, juce::Rectangle<int> area = {}, float scale = 2.0f)
{
    if (snapshotFolder().isEmpty())
        return;
    writeSnapshot (component.createComponentSnapshot (area.isEmpty() ? component.getLocalBounds() : area, true, scale), name);
}

// A width x height image that paint draws on, ready to read: a software image whose Graphics is gone
// before it's returned. A GPU-backed image (Direct2D on Windows) holds its drawing until its context
// ends, so a pixel read while the Graphics is open sees nothing there, on Windows only. The one place
// a test opens a Graphics on an image.
template <typename Paint>
juce::Image paintImage (int width, int height, Paint&& paint)
{
    juce::Image image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        paint (g);
    }
    return image;
}

// A mouse event on target at position, in its own pixels, with mods held: pressed at downAt
// (position unless given), and the clicks-th click.
inline juce::MouseEvent mouseEvent (juce::Component& target, juce::Point<float> position, juce::ModifierKeys mods = {},
                                    std::optional<juce::Point<float>> downAt = std::nullopt, int clicks = 1)
{
    const auto now = juce::Time::getCurrentTime();
    const auto pressedAt = downAt.value_or (position);
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             mods,
             juce::MouseInputSource::defaultPressure,
             juce::MouseInputSource::defaultOrientation,
             juce::MouseInputSource::defaultRotation,
             juce::MouseInputSource::defaultTiltX,
             juce::MouseInputSource::defaultTiltY,
             &target,
             &target,
             now,
             pressedAt,
             now,
             clicks,
             position != pressedAt };
}

// The key with Shift held.
inline juce::KeyPress withShift (juce::KeyPress key) { return { key.getKeyCode(), juce::ModifierKeys::shiftModifier, 0 }; }

// A layout with the given main input and output, and Sidechain (disabled unless given).
inline juce::AudioProcessor::BusesLayout layoutOf (const juce::AudioChannelSet& in,
                                                  const juce::AudioChannelSet& out,
                                                  const juce::AudioChannelSet& sidechain = juce::AudioChannelSet::disabled())
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (in);
    layout.inputBuses.add (sidechain);
    layout.outputBuses.add (out);
    return layout;
}

// Switches the main input and output to channels, with sidechain, and prepares to play at 48 kHz in
// blocks of 512, as a host does.
inline void useLayout (juce::AudioProcessor& processor,
                       const juce::AudioChannelSet& channels,
                       const juce::AudioChannelSet& sidechain = juce::AudioChannelSet::disabled())
{
    REQUIRE (processor.setBusesLayout (layoutOf (channels, channels, sidechain)));
    processor.prepareToPlay (48000.0, 512);
}

// Lets JUCE's timers run for milliseconds: until a timer started now with that interval has fired.
// JUCE fires timers in the order they fall due, so every timer due sooner has fired by then, however
// late a busy machine's timer thread runs them (giving up after 10 s). Tests wait with this, never a
// loop of their own: a wall-clock wait ends before the timers run on a loaded CI runner.
inline void settle (int milliseconds = 60)
{
    struct Probe final : juce::Timer
    {
        bool fired = false;
        void timerCallback() override
        {
            fired = true;
            stopTimer();
        }
    } probe;
    probe.startTimer (milliseconds);
    const auto giveUp = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (milliseconds + 10000);
    while (! probe.fired && juce::Time::getMillisecondCounter() < giveUp)
    {
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (5);
    }
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

    // Lets the editor's timers run for milliseconds, so it shows what the parameters hold (harness::settle).
    void settle (int milliseconds = 60) { harness::settle (milliseconds); }

    // Where the display draws a Band at frequency with Gain 0: on a log scale from 10 Hz to 30 kHz,
    // halfway down.
    juce::Point<float> at (double frequency) const
    {
        return { static_cast<float> (std::log (frequency / 10.0) / std::log (3000.0) * display.getWidth()),
                 static_cast<float> (display.getHeight()) * 0.5f };
    }

    // A mouse event on the display (harness::mouseEvent).
    juce::MouseEvent mouseEvent (juce::Point<float> position, juce::ModifierKeys mods, juce::Point<float> downAt)
    {
        return harness::mouseEvent (display, position, mods, downAt);
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
