// The gate every Factory Preset passes (spec #1, "Factory Presets"). Each rule is a function of one
// Preset, or of the library for unique names, giving what is wrong or nothing; the test runs each
// over the real library and shows it rejects a bad example.

#include "Parameters.h"
#include "PluginProcessor.h"
#include "PresetLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <random>
#include <set>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using eq1::PresetLibrary;

namespace
{

using FactoryPreset = PresetLibrary::FactoryPreset;

// The Factory library's names are "<Category> – <Name>", with an en dash (U+2013).
const juce::String dash = juce::String::fromUTF8 (" \xe2\x80\x93 ");

juce::String named (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// A plugin as a host has it.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;

    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }
};

// Rule: apart from Default, the name is "<Category> – <Name>", the Category one of the spec's.
juce::String namingProblem (const FactoryPreset& preset)
{
    static const juce::StringArray categories { "Utility", "Drums", "Bass",   "Vocals",  "Guitar", "Keys",
                                                "Synth",   "Orchestral", "Mix Bus", "Master", "Sends" };
    if (preset.name == "Default")
        return {};
    if (! preset.name.contains (dash))
        return "not named <Category>" + dash + "<Name>";
    const auto category = preset.name.upToFirstOccurrenceOf (dash, false, false);
    const auto name = preset.name.fromFirstOccurrenceOf (dash, false, false);
    if (! categories.contains (category))
        return "no such Category: " + category;
    if (name.trim().isEmpty() || name != name.trim() || name.contains (dash))
        return "no single Name after the Category";
    return {};
}

// Rule: no two Factory Presets share a name.
juce::String duplicateName (const std::vector<FactoryPreset>& library)
{
    std::set<juce::String> names;
    for (const auto& preset : library)
        if (! names.insert (preset.name).second)
            return "named twice: " + preset.name;
    return {};
}

// Rule: it loads, and every setting it holds is restored.
juce::String loadingProblem (const FactoryPreset& preset)
{
    if (! preset.preset.hasType ("eq1"))
        return "not an eq1 Preset";
    Host host;
    if (! host.processor.loadPreset (preset.preset, preset.name))
        return "doesn't load";
    for (const auto& setting : preset.preset)
    {
        const auto id = setting.getProperty ("id").toString();
        if (host.processor.parameterState().getParameter (id) == nullptr)
            return "no such setting: " + id;
        const auto expected = static_cast<float> (setting.getProperty ("value"));
        const auto restored = host.value (id);
        if (std::abs (restored - expected) > std::max (1.0e-4f, std::abs (expected) * 1.0e-6f))
            return id + " is " + juce::String (restored) + ", not " + juce::String (expected);
    }
    return {};
}

// Rule: it is written at the current state version, holding only the settings that differ from their
// defaults.
juce::String formatProblem (const FactoryPreset& preset)
{
    if (static_cast<int> (preset.preset.getProperty ("version", 0)) != eq1::PluginProcessor::stateVersion)
        return "not at state version " + juce::String (eq1::PluginProcessor::stateVersion);
    Host host;
    for (const auto& setting : preset.preset)
    {
        const auto id = setting.getProperty ("id").toString();
        if (const auto* parameter = host.processor.parameterState().getParameter (id);
            parameter != nullptr
            && juce::exactlyEqual (parameter->convertFrom0to1 (parameter->getDefaultValue()), static_cast<float> (setting.getProperty ("value"))))
            return id + " is at its default";
    }
    return {};
}

// The Factory Presets allowed more Bands than the Band Cap.
const juce::StringArray& pastBandCap()
{
    static const juce::StringArray names { named ("Vocals \xe2\x80\x93 Resonance Control"), named ("Mix Bus \xe2\x80\x93 Drum Bus Sculpt"),
                                           named ("Master \xe2\x80\x93 Detailed") };
    return names;
}

// Rule: at most 6 Bands in use (the Band Cap), unless it is one of pastBandCap().
juce::String bandCountProblem (const FactoryPreset& preset)
{
    constexpr int bandCap = 6;
    Host host;
    host.processor.loadPreset (preset.preset, preset.name);
    int inUse = 0;
    for (int slot = 1; slot <= eq1::numBandSlots; ++slot)
        if (host.value (eq1::parameters::inUseId (slot)) >= 0.5f)
            ++inUse;
    if (inUse > bandCap && ! pastBandCap().contains (preset.name))
        return juce::String (inUse) + " Bands in use";
    return {};
}

// Rule: Gain Scale, Output Pan, Pan Mode and Phase Invert at their defaults. Auto Gain and Output
// Gain may be set.
juce::String outputProblem (const FactoryPreset& preset)
{
    Host host;
    host.processor.loadPreset (preset.preset, preset.name);
    namespace p = eq1::parameters;
    for (const auto& id : { p::gainScaleId, p::outputPanId, p::panModeId, p::phaseInvertId })
    {
        const auto* parameter = host.processor.parameterState().getParameter (id);
        if (! juce::exactlyEqual (parameter->getValue(), parameter->getDefaultValue()))
            return id + " is not at its default";
    }
    return {};
}

// Stereo pink noise at -18 dBFS RMS, each channel its own: white noise through Paul Kellet's pink filter.
juce::AudioBuffer<float> pinkNoise (int channels, int samples)
{
    juce::AudioBuffer<float> noise (channels, samples);
    for (int ch = 0; ch < channels; ++ch)
    {
        std::mt19937 random (static_cast<unsigned> (17 + ch));
        std::normal_distribution<double> white (0.0, 1.0);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, power = 0;
        for (int i = 0; i < samples; ++i)
        {
            const double w = white (random);
            b0 = 0.99886 * b0 + w * 0.0555179;
            b1 = 0.99332 * b1 + w * 0.0750759;
            b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856;
            b4 = 0.55000 * b4 + w * 0.5329522;
            b5 = -0.7616 * b5 - w * 0.0168980;
            const double s = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
            b6 = w * 0.115926;
            noise.setSample (ch, i, static_cast<float> (s));
            power += s * s;
        }
        noise.applyGain (ch, 0, samples, static_cast<float> (juce::Decibels::decibelsToGain (-18.0) / std::sqrt (power / samples)));
    }
    return noise;
}

// Rule: 2 s of stereo pink noise at -18 dBFS, at 48 kHz, comes out finite, its peak no more than 12 dB
// above the input's.
juce::String loudnessProblem (const FactoryPreset& preset)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512, seconds = 2;
    Host host;
    auto& processor = host.processor;
    processor.prepareToPlay (sampleRate, blockSize);
    processor.loadPreset (preset.preset, preset.name);
    const int mainChannels = processor.getMainBusNumInputChannels();
    const auto input = pinkNoise (mainChannels, static_cast<int> (sampleRate) * seconds);
    juce::AudioBuffer<float> buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    float inputPeak = 0.0f, outputPeak = 0.0f;
    for (int start = 0; start + blockSize <= input.getNumSamples(); start += blockSize)
    {
        buffer.clear();
        for (int ch = 0; ch < mainChannels; ++ch)
        {
            buffer.copyFrom (ch, 0, input, ch, start, blockSize);
            inputPeak = std::max (inputPeak, input.getMagnitude (ch, start, blockSize));
        }
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < processor.getMainBusNumOutputChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const auto sample = buffer.getSample (ch, i);
                if (! std::isfinite (sample))
                    return "output not finite";
                outputPeak = std::max (outputPeak, std::abs (sample));
            }
    }
    if (const auto overDb = juce::Decibels::gainToDecibels (outputPeak / inputPeak); overDb > 12.0f)
        return "peak " + juce::String (overDb, 1) + " dB above the input's";
    return {};
}

FactoryPreset preset (const char* nameUtf8, std::initializer_list<std::pair<const char*, float>> settings = {})
{
    juce::ValueTree tree ("eq1");
    tree.setProperty ("version", eq1::PluginProcessor::stateVersion, nullptr);
    for (const auto& [id, value] : settings)
        tree.appendChild (juce::ValueTree ("PARAM").setProperty ("id", id, nullptr).setProperty ("value", value, nullptr), nullptr);
    return { named (nameUtf8), tree };
}

} // namespace

TEST_CASE ("Every Factory Preset passes the gate")
{
    const auto factory = PresetLibrary::factoryPresets();
    REQUIRE (factory.size() >= 3);
    CHECK (duplicateName (factory).isEmpty());
    for (const auto& preset : factory)
    {
        CAPTURE (preset.name);
        CHECK (loadingProblem (preset) == "");
        CHECK (namingProblem (preset) == "");
        CHECK (formatProblem (preset) == "");
        CHECK (bandCountProblem (preset) == "");
        CHECK (outputProblem (preset) == "");
        CHECK (loudnessProblem (preset) == "");
    }
}

TEST_CASE ("Every file in the Factory folder is bundled as a Factory Preset")
{
    juce::StringArray files;
    for (const auto& file : juce::File (juce::CharPointer_UTF8 (EQ1_FACTORY_PRESETS)).findChildFiles (juce::File::findFiles, false, "*" + PresetLibrary::fileExtension))
        files.add (file.getFileNameWithoutExtension());
    juce::StringArray bundled;
    for (const auto& preset : PresetLibrary::factoryPresets())
        bundled.add (preset.name);
    files.sort (false);
    bundled.sort (false);
    REQUIRE (files.size() >= 3);
    CHECK (bundled == files);
}

TEST_CASE ("The Factory Presets are renamed into Categories, Default kept")
{
    juce::StringArray names;
    for (const auto& preset : PresetLibrary::factoryPresets())
        names.add (preset.name);
    for (const auto* name : { "Default", "Drums \xe2\x80\x93 Kick In Punch", "Vocals \xe2\x80\x93 De-Esser", "Vocals \xe2\x80\x93 Lead Presence",
                              "Master \xe2\x80\x93 Polish" })
        CHECK (names.contains (named (name)));
    for (const auto* old : { "Kick Punch", "De-Esser", "Vocal Presence", "Master Polish" })
        CHECK_FALSE (names.contains (old));
}

TEST_CASE ("The Factory library has the Utility and Drums Presets")
{
    juce::StringArray names;
    for (const auto& preset : PresetLibrary::factoryPresets())
        names.add (preset.name);
    for (const auto* name : { "Utility \xe2\x80\x93 Rumble Cut", "Utility \xe2\x80\x93 Low Cut 80 Hz", "Utility \xe2\x80\x93 Mono Bass",
                              "Utility \xe2\x80\x93 Telephone", "Utility \xe2\x80\x93 Tilt Brighter", "Drums \xe2\x80\x93 Kick In Punch",
                              "Drums \xe2\x80\x93 Kick Out Sub", "Drums \xe2\x80\x93 Snare Top Crack", "Drums \xe2\x80\x93 Snare Ring Tamer",
                              "Drums \xe2\x80\x93 Toms Punch", "Drums \xe2\x80\x93 Overheads Air", "Drums \xe2\x80\x93 Room Darken" })
        CHECK (names.contains (named (name)));
}

TEST_CASE ("The Factory library has the Guitar, Keys, Synth and Orchestral Presets")
{
    juce::StringArray names;
    for (const auto& preset : PresetLibrary::factoryPresets())
        names.add (preset.name);
    for (const auto* name : { "Guitar \xe2\x80\x93 Acoustic Body & Sparkle", "Guitar \xe2\x80\x93 Acoustic Boom Control",
                              "Guitar \xe2\x80\x93 Electric Cut Through", "Guitar \xe2\x80\x93 Electric Fizz Tamer", "Keys \xe2\x80\x93 Piano Bright",
                              "Keys \xe2\x80\x93 Piano Mud Cut", "Keys \xe2\x80\x93 Rhodes Warmth", "Synth \xe2\x80\x93 Pad Make Room",
                              "Synth \xe2\x80\x93 Lead Bite", "Orchestral \xe2\x80\x93 Strings Smooth", "Orchestral \xe2\x80\x93 Brass Tame Blare" })
        CHECK (names.contains (named (name)));
}

TEST_CASE ("The Factory gate rejects a name outside the scheme")
{
    CHECK (namingProblem (preset ("Default")) == "");
    CHECK (namingProblem (preset ("Mix Bus \xe2\x80\x93 Glue")) == "");
    CHECK (namingProblem (preset ("Kick Punch")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums - Kick")).isNotEmpty()); // a hyphen, not an en dash
    CHECK (namingProblem (preset ("Drums\xe2\x80\x93Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drum \xe2\x80\x93 Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums \xe2\x80\x93  Kick")).isNotEmpty());
    CHECK (namingProblem (preset ("Drums \xe2\x80\x93 ")).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects two Presets of one name")
{
    CHECK (duplicateName ({ preset ("Drums \xe2\x80\x93 Kick"), preset ("Drums \xe2\x80\x93 Snare") }) == "");
    CHECK (duplicateName ({ preset ("Drums \xe2\x80\x93 Kick"), preset ("Drums \xe2\x80\x93 Kick") }).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects a Preset that doesn't load as written")
{
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f }, { "band1_frequency", 60.0f } })) == "");
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_frequency", 50000.0f } })).isNotEmpty()); // beyond its range
    CHECK (loadingProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_nonsense", 1.0f } })).isNotEmpty());
    CHECK (loadingProblem ({ named ("Drums \xe2\x80\x93 Kick"), juce::ValueTree ("other") }).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects a Preset at an older version, or holding a default")
{
    CHECK (formatProblem (preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f }, { "output_gain", 3.0f } })) == "");
    CHECK (formatProblem (preset ("Drums \xe2\x80\x93 Kick", { { "output_gain", 0.0f } })).isNotEmpty());
    auto older = preset ("Drums \xe2\x80\x93 Kick", { { "band1_in_use", 1.0f } });
    older.preset.setProperty ("version", eq1::PluginProcessor::stateVersion - 1, nullptr);
    CHECK (formatProblem (older).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects more than 6 Bands, but for the three Presets allowed them")
{
    const auto bands = [] (const char* name, int count) {
        auto made = preset (name);
        for (int slot = 1; slot <= count; ++slot)
            made.preset.appendChild (juce::ValueTree ("PARAM").setProperty ("id", eq1::parameters::inUseId (slot), nullptr).setProperty ("value", 1.0f, nullptr),
                                     nullptr);
        return made;
    };
    CHECK (bandCountProblem (bands ("Drums \xe2\x80\x93 Kick", 6)) == "");
    CHECK (bandCountProblem (bands ("Drums \xe2\x80\x93 Kick", 7)).isNotEmpty());
    for (const auto* allowed : { "Vocals \xe2\x80\x93 Resonance Control", "Mix Bus \xe2\x80\x93 Drum Bus Sculpt", "Master \xe2\x80\x93 Detailed" })
        CHECK (bandCountProblem (bands (allowed, 12)) == "");
}

TEST_CASE ("The Factory gate rejects Gain Scale, Output Pan, Pan Mode or Phase Invert away from their defaults")
{
    CHECK (outputProblem (preset ("Master \xe2\x80\x93 Loud", { { "auto_gain", 1.0f }, { "output_gain", 3.0f } })) == "");
    CHECK (outputProblem (preset ("Master \xe2\x80\x93 Loud", { { "gain_scale", 50.0f } })).isNotEmpty());
    CHECK (outputProblem (preset ("Master \xe2\x80\x93 Loud", { { "output_pan", 20.0f } })).isNotEmpty());
    CHECK (outputProblem (preset ("Master \xe2\x80\x93 Loud", { { "pan_mode", 1.0f } })).isNotEmpty());
    CHECK (outputProblem (preset ("Master \xe2\x80\x93 Loud", { { "phase_invert", 1.0f } })).isNotEmpty());
}

TEST_CASE ("The Factory gate rejects a Preset that lifts pink noise's peak by more than 12 dB")
{
    CHECK (loudnessProblem (preset ("Master \xe2\x80\x93 Loud", { { "output_gain", 9.0f } })) == "");
    CHECK (loudnessProblem (preset ("Master \xe2\x80\x93 Loud", { { "output_gain", 15.0f } })).isNotEmpty());
    CHECK (loudnessProblem (preset ("Bass \xe2\x80\x93 Boom", { { "band1_in_use", 1.0f }, { "band1_frequency", 100.0f }, { "band1_gain", 30.0f }, { "band1_q", 0.3f } }))
               .isNotEmpty());
}
