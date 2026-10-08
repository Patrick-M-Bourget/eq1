#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{

// The Shapes in Pro-Q 4's order (ADR 0003).
const juce::StringArray shapeNames { "Bell",      "Low Shelf", "Low Cut",    "High Shelf", "High Cut",
                                     "Notch",     "Band Pass", "Tilt Shelf", "Flat Tilt",  "All Pass" };

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void setParameter (juce::AudioProcessor& processor, const juce::String& id, float value)
{
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && ranged->getParameterID() == id)
            return ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
    FAIL ("No parameter " << id);
}

// Which part of a stereo signal the sine is in: the same on both channels, or inverted on the right.
enum class Content
{
    mid,
    side,
};

// Level change in dB of a steady sine through the processor, measured on the first channel.
double sineGainDb (juce::AudioProcessor& processor, double frequency, Content content = Content::mid)
{
    const int numChannels = processor.getTotalNumInputChannels();
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;
    double inputPower = 0.0, outputPower = 0.0;
    int n = 0;
    for (int block = 0; block < 64; ++block)
    {
        for (int i = 0; i < blockSize; ++i, ++n)
        {
            const auto s = static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * n / sampleRate));
            buffer.setSample (0, i, s);
            if (numChannels > 1)
                buffer.setSample (1, i, content == Content::side ? -s : s);
            if (block >= 32)
                inputPower += s * s;
        }
        processor.processBlock (buffer, midi);
        if (block >= 32)
            for (int i = 0; i < blockSize; ++i)
                outputPower += buffer.getSample (0, i) * buffer.getSample (0, i);
    }
    return 10.0 * std::log10 (outputPower / inputPower);
}

// Switches the main input and output to the given layout, as a host does, and prepares to play.
void useLayout (juce::AudioProcessor& processor, const juce::AudioChannelSet& channels)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channels);
    layout.outputBuses.add (channels);
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (sampleRate, blockSize);
}

} // namespace

TEST_CASE ("Plugin reports zero latency")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Moving the Bell's Gain changes the sound at its Frequency")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    setParameter (processor, "band1_in_use", 1.0f);
    setParameter (processor, "band1_frequency", 1000.0f);
    setParameter (processor, "band1_q", 1.0f);

    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band1_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    setParameter (processor, "band1_gain", -9.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (-9.0, 0.1));
}

TEST_CASE ("The host parameter layout is pinned: IDs, names, ranges, steps, defaults and choices")
{
    // Hosts store parameter IDs in sessions and automation as normalised (0 to 1) values, so after
    // release every field below is frozen: changing one remaps users' saved automation. A change
    // here must be deliberate and say so in its commit. Shape and Slope are frozen by ADR 0003.
    struct Control
    {
        const char* suffix;
        const char* name;
        const char* label;
        float start, end, interval;
        float defaultValue;
        float valueAtHalfway; // pins the mapping between normalised and plain values
        juce::StringArray choices;
    };
    const Control controls[] = {
        { "frequency", "Frequency", "Hz", 10.0f, 30000.0f, 0.0f, 1000.0f, 547.7226f, {} },
        { "gain", "Gain", "dB", -30.0f, 30.0f, 0.0f, 0.0f, 0.0f, {} },
        { "q", "Q", "", 0.025f, 40.0f, 0.0f, 1.0f, 1.0f, {} },
        { "in_use", "In Use", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
        { "bypass", "Bypass", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
        { "shape", "Shape", "", 0.0f, 9.0f, 1.0f, 0.0f, 4.0f, shapeNames },
        { "slope", "Slope", "dB/oct", 0.0f, 96.0f, 0.0f, 12.0f, 48.0f, {} },
        { "brickwall", "Brickwall", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
        { "placement", "Stereo Placement", "", 0.0f, 4.0f, 1.0f, 0.0f, 2.0f, { "Stereo", "Left", "Right", "Mid", "Side" } },
        { "dynamic_range", "Dynamic Range", "dB", -30.0f, 30.0f, 0.0f, 0.0f, 0.0f, {} },
        // Auto Threshold is its own switch, not the top of Threshold (ADR 0003, Consequences).
        { "threshold", "Threshold", "dB", -60.0f, 0.0f, 0.0f, -30.0f, -30.0f, {} },
        { "threshold_auto", "Auto Threshold", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, {} },
        // 50% is Auto: the centre of the range, so not a special value at its end.
        { "attack", "Attack", "%", 0.0f, 100.0f, 0.0f, 50.0f, 50.0f, {} },
        { "release", "Release", "%", 0.0f, 100.0f, 0.0f, 50.0f, 50.0f, {} },
        { "dynamics_bypass", "Dynamics Bypass", "", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, {} },
    };

    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    std::map<juce::String, juce::RangedAudioParameter*> parameters;
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            parameters[ranged->getParameterID()] = ranged;

    CHECK (parameters.size() == 24 * std::size (controls));
    for (int slot = 1; slot <= 24; ++slot)
        for (const auto& control : controls)
        {
            const auto id = "band" + juce::String (slot) + "_" + control.suffix;
            CAPTURE (id);
            REQUIRE (parameters.contains (id));
            auto* parameter = parameters[id];
            const auto& range = parameter->getNormalisableRange();

            CHECK (parameter->getName (100) == "Band " + juce::String (slot) + " " + control.name);
            CHECK (parameter->getLabel() == control.label);
            CHECK (range.start == control.start);
            CHECK (range.end == control.end);
            CHECK (range.interval == control.interval);
            CHECK_THAT (parameter->convertFrom0to1 (parameter->getDefaultValue()), WithinAbs (control.defaultValue, 1.0e-4));
            CHECK_THAT (parameter->convertFrom0to1 (0.5f), WithinAbs (control.valueAtHalfway, 1.0e-3));

            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter);
            CHECK ((choice != nullptr) == ! control.choices.isEmpty());
            if (choice != nullptr)
            {
                CHECK (choice->choices == control.choices);
                for (int index = 0; index < control.choices.size(); ++index)
                    CHECK_THAT (parameter->convertTo0to1 (static_cast<float> (index)),
                                WithinAbs (index / (control.choices.size() - 1.0), 1.0e-6));
            }
            if (juce::String (control.suffix) == "slope")
            {
                CHECK (parameter->convertTo0to1 (0.0f) == 0.0f);
                CHECK (parameter->convertTo0to1 (96.0f) == 1.0f);
            }
        }
}

TEST_CASE ("A Band slot shapes the sound only when in use and not Bypassed")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    setParameter (processor, "band24_frequency", 1000.0f);
    setParameter (processor, "band24_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band24_in_use", 1.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    setParameter (processor, "band24_bypass", 1.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    setParameter (processor, "band24_bypass", 0.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));
}

TEST_CASE ("A producer can pick any Shape for a Band")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioParameterChoice* shape = nullptr;
    for (auto* parameter : processor.getParameters())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter); choice != nullptr && choice->getParameterID() == "band5_shape")
            shape = choice;
    REQUIRE (shape != nullptr);
    CHECK (shape->choices == shapeNames);

    setParameter (processor, "band5_in_use", 1.0f);
    setParameter (processor, "band5_frequency", 1000.0f);
    setParameter (processor, "band5_gain", 12.0f);
    setParameter (processor, "band5_slope", 24.0f);

    const auto pick = [&] (const char* name) { *shape = shape->choices.indexOf (name); };

    pick ("Low Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (12.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (0.0, 0.1));

    pick ("High Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (0.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (12.0, 0.1));

    pick ("Tilt Shelf");
    CHECK_THAT (sineGainDb (processor, 100.0), WithinAbs (-6.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 10000.0), WithinAbs (6.0, 0.1));

    pick ("Flat Tilt");
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 2000.0), WithinAbs (1.2, 0.1));
}

TEST_CASE ("Saved state restores every Band setting, including Brickwall and dynamics")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    setParameter (saved, "band7_in_use", 1.0f);
    setParameter (saved, "band7_shape", 4.0f); // High Cut
    setParameter (saved, "band7_frequency", 8000.0f);
    setParameter (saved, "band7_gain", -4.5f);
    setParameter (saved, "band7_q", 2.5f);
    setParameter (saved, "band7_slope", 37.5f);
    setParameter (saved, "band7_brickwall", 1.0f);
    setParameter (saved, "band7_placement", 3.0f); // Mid
    setParameter (saved, "band24_bypass", 1.0f);
    setParameter (saved, "band7_dynamic_range", -7.5f);
    setParameter (saved, "band7_threshold", -42.0f);
    setParameter (saved, "band7_threshold_auto", 0.0f);
    setParameter (saved, "band7_attack", 20.0f);
    setParameter (saved, "band7_release", 80.0f);
    setParameter (saved, "band7_dynamics_bypass", 1.0f);

    juce::MemoryBlock state;
    saved.getStateInformation (state);
    eq1::PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto savedParameters = saved.getParameters();
    const auto restoredParameters = restored.getParameters();
    REQUIRE (savedParameters.size() == restoredParameters.size());
    for (int i = 0; i < savedParameters.size(); ++i)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (restoredParameters[i]);
        REQUIRE (parameter != nullptr);
        CAPTURE (parameter->getParameterID());
        CHECK (parameter->getValue() == savedParameters[i]->getValue());
    }

    const auto value = [&] (const juce::String& id) {
        for (auto* parameter : restoredParameters)
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged->getParameterID() == id)
                return ranged->convertFrom0to1 (ranged->getValue());
        FAIL ("No parameter " << id);
        return 0.0f;
    };
    CHECK (value ("band7_brickwall") == 1.0f);
    CHECK (value ("band7_placement") == 3.0f);
    CHECK (value ("band7_shape") == 4.0f);
    CHECK_THAT (value ("band7_slope"), WithinAbs (37.5, 1.0e-4));
    CHECK_THAT (value ("band7_dynamic_range"), WithinAbs (-7.5, 1.0e-4));
    CHECK_THAT (value ("band7_threshold"), WithinAbs (-42.0, 1.0e-4));
    CHECK (value ("band7_threshold_auto") == 0.0f);
    CHECK_THAT (value ("band7_attack"), WithinAbs (20.0, 1.0e-4));
    CHECK_THAT (value ("band7_release"), WithinAbs (80.0, 1.0e-4));
    CHECK (value ("band7_dynamics_bypass") == 1.0f);
}

TEST_CASE ("Hosts can use the plugin on mono and stereo tracks")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();

    const auto supports = [&] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out) {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (in);
        layout.outputBuses.add (out);
        return processor.checkBusesLayoutSupported (layout);
    };
    CHECK (supports (mono, mono));
    CHECK (supports (stereo, stereo));
    CHECK_FALSE (supports (mono, stereo));
    CHECK_FALSE (supports (stereo, mono));
}

TEST_CASE ("Stereo Placement is available on stereo tracks only")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    useLayout (processor, juce::AudioChannelSet::stereo());
    CHECK (processor.isStereoPlacementAvailable());
    useLayout (processor, juce::AudioChannelSet::mono());
    CHECK_FALSE (processor.isStereoPlacementAvailable());
}

TEST_CASE ("On a mono track a Side Band has no effect, and its Stereo Placement comes back on stereo")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    useLayout (processor, juce::AudioChannelSet::stereo());

    setParameter (processor, "band3_in_use", 1.0f);
    setParameter (processor, "band3_frequency", 1000.0f);
    setParameter (processor, "band3_gain", 12.0f);
    setParameter (processor, "band3_placement", 4.0f); // Side
    CHECK_THAT (sineGainDb (processor, 1000.0, Content::side), WithinAbs (12.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));

    useLayout (processor, juce::AudioChannelSet::mono());
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));
    setParameter (processor, "band3_placement", 3.0f); // Mid
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));
    setParameter (processor, "band3_placement", 4.0f);

    useLayout (processor, juce::AudioChannelSet::stereo());
    CHECK_THAT (sineGainDb (processor, 1000.0, Content::side), WithinAbs (12.0, 0.1));
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.05));
}

TEST_CASE ("Holding Solo plays only the Band's region, and letting go restores the EQ")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameter (processor, "band3_in_use", 1.0f);
    setParameter (processor, "band3_frequency", 1000.0f);
    setParameter (processor, "band3_gain", 12.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));

    processor.setSolo (3);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (0.0, 0.1));
    CHECK (sineGainDb (processor, 100.0) < -15.0);

    processor.setSolo (0);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (12.0, 0.1));
}

TEST_CASE ("Solo is not a host parameter and is not saved with the session")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    for (auto* parameter : saved.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            CHECK_FALSE (ranged->getParameterID().containsIgnoreCase ("solo"));

    setParameter (saved, "band3_in_use", 1.0f);
    saved.setSolo (3);
    juce::MemoryBlock state;
    saved.getStateInformation (state);
    CHECK_FALSE (state.toString().containsIgnoreCase ("solo"));

    eq1::PluginProcessor restored;
    restored.setSolo (5);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    // Restoring a session lets go of any Solo.
    CHECK (restored.soloSlot() == 0);
}

TEST_CASE ("A Dynamic Band on the host parameters moves its Live Gain, which the editor can read")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameter (processor, "band2_in_use", 1.0f);
    setParameter (processor, "band2_gain", 4.0f);
    setParameter (processor, "band2_threshold_auto", 0.0f);
    setParameter (processor, "band2_threshold", -40.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (4.0, 0.1));
    CHECK_THAT (processor.liveGainDb (2), WithinAbs (4.0, 1.0e-4));

    setParameter (processor, "band2_dynamic_range", -10.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (-6.0, 0.1));
    CHECK_THAT (processor.liveGainDb (2), WithinAbs (-6.0, 0.05));

    setParameter (processor, "band2_dynamics_bypass", 1.0f);
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (4.0, 0.1));
}

TEST_CASE ("Switching a Dynamic Band to a Shape without dynamics keeps its dynamics settings")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameter (processor, "band1_in_use", 1.0f);
    setParameter (processor, "band1_dynamic_range", -10.0f);
    setParameter (processor, "band1_threshold_auto", 0.0f);
    setParameter (processor, "band1_threshold", -40.0f);
    setParameter (processor, "band1_shape", 5.0f); // Notch
    sineGainDb (processor, 1000.0);
    setParameter (processor, "band1_shape", 0.0f); // Bell
    CHECK_THAT (sineGainDb (processor, 1000.0), WithinAbs (-10.0, 0.1));
}
