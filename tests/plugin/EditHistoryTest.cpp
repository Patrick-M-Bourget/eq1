#include "BandEditing.h"
#include "EditHistory.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::Shape;

namespace
{

// A plugin as a host has it, with the editor's editing rules and its undo history.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::EditHistory& history = processor.editHistory();
    eq1::BandEditing editing { processor.parameterState(), history };

    juce::RangedAudioParameter& parameter (const juce::String& id) { return *processor.parameterState().getParameter (id); }
    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }
    float value (int slot, const char* control) { return value ("band" + juce::String (slot) + "_" + control); }

    // A change from the host: automation playback or its own parameter view. No gesture.
    void hostSets (const juce::String& id, float plain)
    {
        auto& p = parameter (id);
        p.setValue (p.convertTo0to1 (plain));
        p.sendValueChangedMessageToListeners (p.convertTo0to1 (plain));
    }
};

// Counts the gestures a host sees.
struct GestureLog final : juce::AudioProcessorListener
{
    int begins = 0, ends = 0;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++begins; }
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override { ++ends; }
};

} // namespace

TEST_CASE ("The undo history starts empty")
{
    Host host;
    CHECK_FALSE (host.history.canUndo());
    CHECK_FALSE (host.history.canRedo());
    CHECK (host.history.undoSteps() == 0);
}

TEST_CASE ("Adding a Band is one undo step: undo takes it away, redo brings it back")
{
    Host host;
    const auto slot = host.editing.add (500.0, 6.0);
    REQUIRE (slot == 1);
    CHECK (host.history.undoSteps() == 1);

    host.history.undo();
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK_THAT (host.value (1, "frequency"), WithinRel (1000.0f, 1.0e-4f));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (0.0, 1.0e-4));
    CHECK (host.history.canRedo());

    host.history.redo();
    CHECK (host.value (1, "in_use") == 1.0f);
    CHECK_THAT (host.value (1, "frequency"), WithinRel (500.0f, 1.0e-4f));
    CHECK_THAT (host.value (1, "gain"), WithinAbs (6.0, 1.0e-4));
}

TEST_CASE ("Deleting a Band, changing its Shape and scaling its Q are one undo step each")
{
    Host host;
    host.editing.add (500.0, 6.0);
    host.editing.setShape (1, Shape::HighShelf);
    host.editing.scaleQ (1, 2.0);
    host.editing.deleteBand (1);
    CHECK (host.history.undoSteps() == 4);

    host.history.undo();
    CHECK (host.value (1, "in_use") == 1.0f);
    host.history.undo();
    CHECK_THAT (host.value (1, "q"), WithinRel (1.0f, 1.0e-4f));
    host.history.undo();
    CHECK (host.value (1, "shape") == 0.0f);
    host.history.undo();
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK_FALSE (host.history.canUndo());

    for (int step = 0; step < 4; ++step)
        host.history.redo();
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK (host.value (1, "shape") == 3.0f);
    CHECK_THAT (host.value (1, "q"), WithinRel (2.0f, 1.0e-4f));
    CHECK_FALSE (host.history.canRedo());
}

TEST_CASE ("Deleting several selected Bands at once is one undo step")
{
    Host host;
    for (double frequency : { 100.0, 1000.0, 5000.0 })
        host.editing.add (frequency, 3.0);
    host.editing.deleteBands ({ 1, 3 });
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK (host.value (2, "in_use") == 1.0f);
    CHECK (host.value (3, "in_use") == 0.0f);
    CHECK (host.history.undoSteps() == 4);
    host.history.undo();
    CHECK (host.value (1, "in_use") == 1.0f);
    CHECK (host.value (3, "in_use") == 1.0f);
}

TEST_CASE ("A drag is one undo step, however far it goes")
{
    Host host;
    host.editing.add (100.0, 0.0);
    host.editing.add (1000.0, 3.0);
    host.editing.beginDrag ({ 1, 2 });
    for (int step = 1; step <= 10; ++step)
        host.editing.dragBy (1.0 + 0.1 * step, 0.5 * step);
    host.editing.endDrag();
    CHECK (host.history.undoSteps() == 3);

    host.history.undo();
    CHECK_THAT (host.value (1, "frequency"), WithinRel (100.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (3.0, 1.0e-4));
    host.history.redo();
    CHECK_THAT (host.value (1, "frequency"), WithinRel (200.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "gain"), WithinAbs (8.0, 1.0e-4));
}

TEST_CASE ("Spectrum Grab and the drag that sets the grabbed Band's Gain are one undo step")
{
    Host host;
    REQUIRE (host.editing.grab (2000.0) == 1);
    host.editing.dragBy (1.0, 9.0);
    host.editing.endDrag();
    CHECK (host.history.undoSteps() == 1);
    host.history.undo();
    CHECK (host.value (1, "in_use") == 0.0f);
    CHECK_THAT (host.value (1, "gain"), WithinAbs (0.0, 1.0e-4));
}

TEST_CASE ("Edits on the Band panel's and output row's controls are undo steps")
{
    Host host;
    auto& state = host.processor.parameterState();

    // A Slider, a ComboBox and a ToggleButton, attached as the editor's controls are.
    juce::Slider outputGain;
    juce::AudioProcessorValueTreeState::SliderAttachment outputGainAttachment (state, eq1::parameters::outputGainId, outputGain);
    juce::ComboBox panMode;
    panMode.addItemList (eq1::parameters::panModeNames(), 1);
    juce::AudioProcessorValueTreeState::ComboBoxAttachment panModeAttachment (state, eq1::parameters::panModeId, panMode);
    juce::ToggleButton phaseInvert;
    juce::AudioProcessorValueTreeState::ButtonAttachment phaseInvertAttachment (state, eq1::parameters::phaseInvertId, phaseInvert);

    {
        // As a mouse drag, typed value, double-click or wheel on the Slider does.
        const juce::Slider::ScopedDragNotification drag (outputGain);
        outputGain.setValue (-6.0, juce::sendNotificationSync);
    }
    panMode.setSelectedItemIndex (1, juce::sendNotificationSync);
    phaseInvert.setToggleState (true, juce::sendNotificationSync);
    CHECK (host.history.undoSteps() == 3);

    host.history.undo();
    CHECK (host.value (eq1::parameters::phaseInvertId) == 0.0f);
    host.history.undo();
    CHECK (host.value (eq1::parameters::panModeId) == 0.0f);
    host.history.undo();
    CHECK_THAT (host.value (eq1::parameters::outputGainId), WithinAbs (0.0, 1.0e-3));
}

TEST_CASE ("Host Automation and the host's own parameter view add no undo steps")
{
    Host host;
    host.editing.add (500.0, 6.0);
    REQUIRE (host.history.undoSteps() == 1);

    host.hostSets ("band1_gain", -12.0f);
    host.hostSets (eq1::parameters::outputGainId, 9.0f);
    host.hostSets ("band5_in_use", 1.0f);
    CHECK (host.history.undoSteps() == 1);
    // Playing automation back while audio runs, too.
    host.processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (host.processor.getTotalNumInputChannels(), 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 4; ++block)
    {
        host.hostSets ("band1_frequency", 600.0f + 100.0f * block);
        host.processor.processBlock (buffer, midi);
    }
    CHECK (host.history.undoSteps() == 1);
}

TEST_CASE ("Undo and redo reach the host as gestures, and are not themselves undo steps")
{
    Host host;
    host.editing.add (500.0, 6.0);
    host.editing.setShape (1, Shape::Notch);
    GestureLog log;
    host.processor.addListener (&log);

    host.history.undo();
    CHECK (log.begins >= 1);
    CHECK (log.begins == log.ends);
    CHECK (host.history.undoSteps() == 1);
    CHECK (host.history.canRedo());
    host.history.redo();
    CHECK (host.history.undoSteps() == 2);
    CHECK_FALSE (host.history.canRedo());
    host.processor.removeListener (&log);
}

TEST_CASE ("A new edit after an undo replaces what could be redone")
{
    Host host;
    host.editing.add (500.0, 6.0);
    host.editing.setShape (1, Shape::Notch);
    host.history.undo();
    REQUIRE (host.history.canRedo());
    host.editing.scaleQ (1, 3.0);
    CHECK_FALSE (host.history.canRedo());
    CHECK (host.history.undoSteps() == 2);
}

TEST_CASE ("The undo history is empty after a session is reloaded")
{
    Host host;
    host.editing.add (500.0, 6.0);
    host.editing.setShape (1, Shape::Notch);
    host.history.undo();
    juce::MemoryBlock state;
    host.processor.getStateInformation (state);
    host.processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK_FALSE (host.history.canUndo());
    CHECK_FALSE (host.history.canRedo());
}

TEST_CASE ("Host Automation during an editor drag stays out of the drag's undo step")
{
    Host host;
    host.editing.add (100.0, 0.0);
    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (2.0, 0.0);
    host.hostSets (eq1::parameters::outputGainId, -9.0f);
    host.hostSets ("band5_gain", 4.0f);
    host.editing.endDrag();
    REQUIRE (host.history.undoSteps() == 2);

    host.history.undo();
    CHECK_THAT (host.value (1, "frequency"), WithinRel (100.0f, 1.0e-4f));
    CHECK_THAT (host.value (eq1::parameters::outputGainId), WithinAbs (-9.0, 1.0e-3));
    CHECK_THAT (host.value (5, "gain"), WithinAbs (4.0, 1.0e-4));
}

TEST_CASE ("A session restored in the middle of a drag leaves the undo history empty")
{
    Host host;
    host.editing.add (100.0, 0.0);
    juce::MemoryBlock state;
    host.processor.getStateInformation (state);

    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (2.0, 3.0);
    host.processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    host.editing.dragBy (3.0, 3.0);
    host.editing.endDrag();
    CHECK_FALSE (host.history.canUndo());
    CHECK_FALSE (host.history.canRedo());

    // The next edit is recorded as usual.
    host.editing.setShape (1, Shape::Notch);
    CHECK (host.history.undoSteps() == 1);
}

TEST_CASE ("Undo and redo wait until an edit in progress is finished")
{
    Host host;
    host.editing.add (100.0, 0.0);
    host.editing.beginDrag ({ 1 });
    host.editing.dragBy (2.0, 0.0);
    host.history.undo();
    CHECK (host.value (1, "in_use") == 1.0f);
    CHECK_THAT (host.value (1, "frequency"), WithinRel (200.0f, 1.0e-4f));
    host.editing.endDrag();
    CHECK (host.history.undoSteps() == 2);

    host.history.undo();
    host.editing.beginDrag ({ 1 });
    host.history.redo();
    CHECK_THAT (host.value (1, "frequency"), WithinRel (100.0f, 1.0e-4f));
    host.editing.endDrag();
    CHECK (host.history.canRedo());
}

TEST_CASE ("The wheel over several selected Bands is one undo step")
{
    Host host;
    host.editing.add (100.0, 0.0);
    host.editing.add (1000.0, 0.0);
    host.editing.scaleQ ({ 1, 2 }, 2.0);
    CHECK_THAT (host.value (2, "q"), WithinRel (2.0f, 1.0e-4f));
    CHECK (host.history.undoSteps() == 3);
    host.history.undo();
    CHECK_THAT (host.value (1, "q"), WithinRel (1.0f, 1.0e-4f));
    CHECK_THAT (host.value (2, "q"), WithinRel (1.0f, 1.0e-4f));
}
