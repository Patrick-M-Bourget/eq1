#include "EditorHarness.h"
#include "PluginProcessor.h"
#include "PresetLibrary.h"
#include "PresetSettings.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <random>

using Catch::Matchers::WithinAbs;

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

juce::RangedAudioParameter& parameter (juce::AudioProcessor& processor, const juce::String& id)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
            return *ranged;
    FAIL ("No parameter " << id);
    throw std::logic_error ("unreachable");
}

float value (juce::AudioProcessor& processor, const juce::String& id)
{
    auto& p = parameter (processor, id);
    return p.convertFrom0to1 (p.getValue());
}

void set (juce::AudioProcessor& processor, const juce::String& id, float plain)
{
    auto& p = parameter (processor, id);
    p.setValueNotifyingHost (p.convertTo0to1 (plain));
}

std::unique_ptr<juce::XmlElement> fixture (const char* name)
{
    auto xml = juce::XmlDocument::parse (juce::File (EQ1_TEST_FIXTURES).getChildFile (name));
    REQUIRE (xml != nullptr);
    return xml;
}

// Two seconds of the same stereo noise, with a Sidechain of other noise, through processor.
juce::AudioBuffer<float> play (juce::AudioProcessor& processor)
{
    processor.prepareToPlay (sampleRate, blockSize);
    std::mt19937 random (3);
    std::uniform_real_distribution<float> unit (-0.5f, 0.5f);
    const int blocks = static_cast<int> (2.0 * sampleRate / blockSize);
    juce::AudioBuffer<float> output (2, blocks * blockSize), buffer (processor.getTotalNumInputChannels(), blockSize);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, unit (random));
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, b * blockSize, buffer, ch, 0, blockSize);
    }
    return output;
}

} // namespace

TEST_CASE ("Saved state carries the state version")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    const auto xml = harness::savedXml (processor);
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("version", -1) == eq1::PluginProcessor::stateVersion);
    CHECK (eq1::PluginProcessor::stateVersion == 3);
}

TEST_CASE ("Save and reload sound exactly the same: Bands, dynamics, Sidechain detection and the output section")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    const std::pair<const char*, float> values[] = {
        { "band1_in_use", 1.0f },         { "band1_frequency", 250.0f },     { "band1_gain", -4.5f },      { "band1_q", 2.0f },
        { "band3_in_use", 1.0f },         { "band3_shape", 4.0f },           { "band3_frequency", 9000.0f }, { "band3_slope", 36.0f },
        { "band3_placement", 3.0f },      { "band7_in_use", 1.0f },          { "band7_frequency", 3000.0f }, { "band7_gain", 2.0f },
        { "band7_dynamic_range", -6.0f }, { "band7_threshold_auto", 0.0f },  { "band7_threshold", -36.0f }, { "band7_detection_source", 1.0f },
        { "gain_scale", 150.0f },         { "auto_gain", 1.0f },             { "output_gain", -3.0f },     { "output_pan", 25.0f },
        { "pan_mode", 1.0f },             { "phase_invert", 1.0f } };
    for (const auto& [id, v] : values)
        set (saved, id, v);
    const auto xml = harness::savedXml (saved);
    REQUIRE (xml != nullptr);

    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    const auto expected = play (saved), actual = play (restored);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < expected.getNumSamples(); ++i)
            REQUIRE (juce::exactlyEqual (actual.getSample (ch, i), expected.getSample (ch, i)));
}

TEST_CASE ("A/B Compare survives save and reload: the side you're on and both sides' settings")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    set (saved, "band1_in_use", 1.0f);
    set (saved, "band1_frequency", 250.0f);
    set (saved, "band1_gain", -4.5f);
    saved.selectCompareSide (eq1::CompareSide::B);
    set (saved, "band1_gain", 9.0f);
    set (saved, "band4_in_use", 1.0f);
    set (saved, "band4_shape", 2.0f);
    set (saved, "output_gain", -2.0f);
    const auto xml = harness::savedXml (saved);
    REQUIRE (xml != nullptr);

    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    CHECK (restored.compareSide() == eq1::CompareSide::B);
    const auto checkSameSound = [&] {
        const auto expected = play (saved), actual = play (restored);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < expected.getNumSamples(); ++i)
                REQUIRE (juce::exactlyEqual (actual.getSample (ch, i), expected.getSample (ch, i)));
    };
    checkSameSound();

    saved.selectCompareSide (eq1::CompareSide::A);
    restored.selectCompareSide (eq1::CompareSide::A);
    CHECK_THAT (value (restored, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (restored, "band4_in_use") == 0.0f);
    checkSameSound();
}

TEST_CASE ("A version 1 session loads on side A, with B a copy of it")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.selectCompareSide (eq1::CompareSide::B);
    harness::restoreState (processor, *fixture ("state-v1.xml"));
    CHECK (processor.compareSide() == eq1::CompareSide::A);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));

    processor.selectCompareSide (eq1::CompareSide::B);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (processor, "band3_shape") == 4.0f);
    CHECK_THAT (value (processor, "gain_scale"), WithinAbs (150.0, 1.0e-3));
}

TEST_CASE ("A session saved before the state had a version (version 0) loads every setting")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    harness::restoreState (processor, *fixture ("state-v0.xml"));

    CHECK (value (processor, "band1_in_use") == 1.0f);
    CHECK_THAT (value (processor, "band1_frequency"), WithinAbs (250.0, 1.0e-3));
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (value (processor, "band3_shape") == 4.0f);
    CHECK (value (processor, "band3_placement") == 3.0f);
    CHECK_THAT (value (processor, "band7_dynamic_range"), WithinAbs (-6.0, 1.0e-4));
    CHECK_THAT (value (processor, "band7_threshold"), WithinAbs (-36.0, 1.0e-4));
    CHECK (value (processor, "band7_detection_source") == 1.0f);
    CHECK (value (processor, "band24_bypass") == 1.0f);
    CHECK_THAT (value (processor, "gain_scale"), WithinAbs (150.0, 1.0e-3));
    CHECK_THAT (value (processor, "output_gain"), WithinAbs (-3.0, 1.0e-3));
    CHECK (value (processor, "pan_mode") == 1.0f);
    CHECK (processor.displayRangeDb() == 30);
    const auto analyzer = processor.analyzerSettings();
    CHECK_FALSE (analyzer.showPreEq);
    CHECK (analyzer.rangeDb == 120);
    CHECK (analyzer.speed == eq1::AnalyzerSpeed::fast);
    // Saved before Peak Hold, it reads as on.
    CHECK (analyzer.peakHold);

    // Saved again, it is the current version.
    CHECK (harness::savedXml (processor)->getIntAttribute ("version", -1) == eq1::PluginProcessor::stateVersion);
}

TEST_CASE ("A session from a newer version loads the settings this version knows")
{
    juce::ScopedJuceInitialiser_GUI juce;
    auto xml = fixture ("state-v0.xml");
    xml->setAttribute ("version", eq1::PluginProcessor::stateVersion + 1);
    auto* unknown = xml->createNewChildElement ("PARAM");
    unknown->setAttribute ("id", "band1_from_the_future");
    unknown->setAttribute ("value", 7.0);
    xml->createNewChildElement ("SomethingNew")->setAttribute ("x", 1);

    eq1::PluginProcessor processor;
    processor.selectCompareSide (eq1::CompareSide::B);
    harness::restoreState (processor, *xml);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (processor.displayRangeDb() == 30);
    CHECK (processor.analyzerSettings().rangeDb == 120);
    // It saves no A/B Compare this version knows: its settings are side A's.
    CHECK (processor.compareSide() == eq1::CompareSide::A);
}

TEST_CASE ("A session saved in the middle of an A/B switch or a Preset load holds both sides whole")
{
    // A host saving from its own thread can save while the editor is switching sides. A listener on
    // the parameters saves at the first change the switch makes, as such a host might.
    struct SaveDuringChange final : juce::AudioProcessorListener
    {
        juce::AudioProcessor& processor;
        std::unique_ptr<juce::XmlElement> saved;
        explicit SaveDuringChange (juce::AudioProcessor& p) : processor (p) { processor.addListener (this); }
        ~SaveDuringChange() override { processor.removeListener (this); }
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override
        {
            if (saved == nullptr)
                saved = harness::savedXml (processor);
        }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    };

    juce::ScopedJuceInitialiser_GUI juce;
    const auto change = GENERATE (Catch::Generators::as<std::string> {}, "switch", "load");
    CAPTURE (change);
    const bool loadOnB = change == "load";
    eq1::PluginProcessor maker;
    set (maker, "band1_gain", 6.0f);
    set (maker, "band2_gain", 2.0f);
    const auto preset = maker.presetState();
    eq1::PluginProcessor processor;
    set (processor, "band1_gain", 6.0f);
    set (processor, "band2_gain", 2.0f);
    processor.selectCompareSide (eq1::CompareSide::B);
    set (processor, "band1_gain", -3.0f);
    set (processor, "band2_gain", -1.0f);
    std::unique_ptr<juce::XmlElement> xml;
    {
        SaveDuringChange host (processor);
        if (loadOnB)
            processor.loadPreset (preset, "Preset");
        else
            processor.selectCompareSide (eq1::CompareSide::A);
        xml = std::move (host.saved);
    }
    REQUIRE (xml != nullptr);

    // Reloaded, the side being switched to is whole on the parameters, and the other side whole too.
    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    // A's settings are on the side the change ends on, and on the other side too after a load.
    const auto ending = loadOnB ? eq1::CompareSide::B : eq1::CompareSide::A;
    CHECK (restored.compareSide() == ending);
    CHECK_THAT (value (restored, "band1_gain"), WithinAbs (6.0, 1.0e-4));
    CHECK_THAT (value (restored, "band2_gain"), WithinAbs (2.0, 1.0e-4));
    if (loadOnB)
    {
        CHECK (restored.loadedPresetName() == "Preset");
        CHECK_FALSE (restored.isLoadedPresetModified());
    }
    restored.selectCompareSide (ending == eq1::CompareSide::B ? eq1::CompareSide::A : eq1::CompareSide::B);
    CHECK_THAT (value (restored, "band1_gain"), WithinAbs (change == "switch" ? -3.0 : 6.0, 1.0e-4));
    CHECK_THAT (value (restored, "band2_gain"), WithinAbs (change == "switch" ? -1.0 : 2.0, 1.0e-4));
}

TEST_CASE ("Save and reload keep each side's Loaded Preset and Modified state, even after its file is deleted")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::TemporaryFile folder;
    REQUIRE (folder.getFile().createDirectory());
    const eq1::PresetLibrary library (folder.getFile());
    const auto savePreset = [&] (const juce::String& name, const std::initializer_list<std::pair<const char*, float>> values) {
        eq1::PluginProcessor maker;
        for (const auto& [id, v] : values)
            set (maker, id, v);
        const auto file = library.save (name, maker.presetState());
        REQUIRE (file.has_value());
        return *file;
    };
    const auto vocal = savePreset ("Vocal", { { "band2_in_use", 1.0f }, { "band2_frequency", 3456.7f }, { "band2_q", 2.7f } });
    const auto kick = savePreset ("Kick", { { "band1_in_use", 1.0f }, { "band1_gain", 4.0f }, { "output_pan", -13.0f } });

    eq1::PluginProcessor saved;
    saved.loadPreset (eq1::PresetLibrary::read (vocal), "Vocal");
    set (saved, "band2_gain", 3.0f);
    saved.selectCompareSide (eq1::CompareSide::B);
    saved.loadPreset (eq1::PresetLibrary::read (kick), "Kick");
    const auto xml = harness::savedXml (saved);
    REQUIRE (xml != nullptr);
    folder.getFile().deleteRecursively();

    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    CHECK (restored.loadedPresetName() == "Kick");
    CHECK_FALSE (restored.isLoadedPresetModified());
    restored.selectCompareSide (eq1::CompareSide::A);
    CHECK (restored.loadedPresetName() == "Vocal");
    CHECK (restored.isLoadedPresetModified());
    set (restored, "band2_gain", 0.0f);
    CHECK_FALSE (restored.isLoadedPresetModified());
}

TEST_CASE ("A version 2 session loads with no Loaded Preset on either side")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor maker;
    set (maker, "band1_in_use", 1.0f);
    eq1::PluginProcessor processor;
    processor.loadPreset (maker.presetState(), "Before");
    processor.selectCompareSide (eq1::CompareSide::B);

    harness::restoreState (processor, *fixture ("state-v2.xml"));
    CHECK (processor.compareSide() == eq1::CompareSide::A);
    CHECK_THAT (value (processor, "band1_gain"), WithinAbs (-4.5, 1.0e-4));
    CHECK (processor.loadedPresetName().isEmpty());
    CHECK_FALSE (processor.isLoadedPresetModified());
    processor.selectCompareSide (eq1::CompareSide::B);
    CHECK (processor.loadedPresetName().isEmpty());
}

TEST_CASE ("Whether the Output Meter is shown is saved with the session, isn't undoable and doesn't make a side Modified")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor saved;
    CHECK (saved.isOutputMeterShown());
    // With a Loaded Preset, a change to any sound setting would make the side Modified.
    REQUIRE (saved.loadPreset (saved.presetState(), "Current"));
    REQUIRE_FALSE (saved.isLoadedPresetModified());
    const int steps = saved.editHistory().undoSteps(); // loading the Preset is one
    saved.setOutputMeterShown (false);
    CHECK_FALSE (saved.isLoadedPresetModified());
    CHECK (saved.editHistory().undoSteps() == steps);
    const auto xml = harness::savedXml (saved);
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("version", -1) == eq1::PluginProcessor::stateVersion);

    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    CHECK_FALSE (restored.isOutputMeterShown());
}

TEST_CASE ("A session saved before the Output Meter loads with it shown")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    processor.setOutputMeterShown (false);
    harness::restoreState (processor, *fixture ("state-v0.xml"));
    CHECK (processor.isOutputMeterShown());
}

TEST_CASE ("The saved names a test checks include nested properties and host parameter ids")
{
    juce::ValueTree state ("eq1");
    state.setProperty ("displayRangeDb", 12, nullptr);
    juce::ValueTree side ("A");
    juce::ValueTree parameter ("PARAM");
    parameter.setProperty ("id", "band3_solo_slot", nullptr);
    side.appendChild (parameter, nullptr);
    juce::ValueTree held ("Held");
    held.setProperty ("auditionSlot", 3, nullptr);
    side.appendChild (held, nullptr);
    state.appendChild (side, nullptr);

    const auto names = harness::savedNames (state);
    CHECK (names.contains ("displayRangeDb"));
    CHECK (names.contains ("band3_solo_slot"));
    CHECK (names.contains ("auditionSlot"));
    CHECK (names.contains ("Held"));
}

namespace
{

// Sets a saved setting's value as written in the file, which may be anything.
void setSaved (juce::XmlElement& state, const juce::String& id, const juce::String& text)
{
    for (auto* child : state.getChildWithTagNameIterator ("PARAM"))
        if (child->getStringAttribute ("id") == id)
        {
            child->setAttribute ("value", text);
            return;
        }
    FAIL ("No saved setting " << id);
}

} // namespace

TEST_CASE ("A state with nan, inf and out-of-range values restores to finite, in-range parameters: defaults where a value isn't finite, the range's end where it is outside")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor maker;
    set (maker, "band1_in_use", 1.0f);
    const auto xml = harness::savedXml (maker);
    REQUIRE (xml != nullptr);
    setSaved (*xml, "band1_frequency", "nan");
    setSaved (*xml, "band1_gain", "inf");
    setSaved (*xml, "band1_q", "-inf");
    setSaved (*xml, "band1_threshold", "NaN");
    setSaved (*xml, "band2_gain", "99");
    setSaved (*xml, "band2_frequency", "1");
    setSaved (*xml, "band2_q", "1e30");
    setSaved (*xml, "band3_shape", "42");
    setSaved (*xml, "band3_slope", "-7");
    setSaved (*xml, "output_gain", "-1000");
    setSaved (*xml, "gain_scale", "not a number");

    eq1::PluginProcessor restored;
    harness::restoreState (restored, *xml);
    REQUIRE (harness::firstParameterOutOfRange (restored).isEmpty());
    // Not finite: the default.
    CHECK_THAT (value (restored, "band1_frequency"), WithinAbs (1000.0, 1.0e-3));
    CHECK (value (restored, "band1_gain") == 0.0f);
    CHECK_THAT (value (restored, "band1_q"), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (value (restored, "band1_threshold"), WithinAbs (-30.0, 1.0e-4));
    // Finite and outside the range: the range's end.
    CHECK_THAT (value (restored, "band2_gain"), WithinAbs (eq1::parameterRanges::gain.high, 1.0e-4));
    CHECK_THAT (value (restored, "band2_frequency"), WithinAbs (eq1::parameterRanges::frequency.low, 1.0e-3));
    CHECK_THAT (value (restored, "band2_q"), WithinAbs (eq1::parameterRanges::q.high, 1.0e-3));
    CHECK (value (restored, "band3_shape") == static_cast<float> (eq1::parameters::shapeNames().size() - 1));
    CHECK (value (restored, "band3_slope") == 0.0f);
    CHECK_THAT (value (restored, "output_gain"), WithinAbs (eq1::parameters::outputGainSilentDb, 1.0e-3));
    // Text that isn't a number reads as 0, which is in Gain Scale's range.
    CHECK (value (restored, "gain_scale") == 0.0f);
    // The rest restored as saved.
    CHECK (value (restored, "band1_in_use") == 1.0f);
}

TEST_CASE ("A restored number is itself within its range, the nearer end outside it, and the default when it isn't a finite number")
{
    using eq1::restoredNumber;
    CHECK (restoredNumber (juce::var (2.5), 1.0, 0.0, 6.0) == 2.5);
    CHECK (restoredNumber (juce::var (0.0), 1.0, 0.0, 6.0) == 0.0);
    CHECK (restoredNumber (juce::var (6.0), 1.0, 0.0, 6.0) == 6.0);
    CHECK (restoredNumber (juce::var (99.0), 1.0, 0.0, 6.0) == 6.0);
    CHECK (restoredNumber (juce::var (-7.0), 1.0, 0.0, 6.0) == 0.0);
    CHECK (restoredNumber (juce::var ("1e39"), 1.0, 0.0, 6.0) == 6.0);
    CHECK (restoredNumber (juce::var (std::numeric_limits<double>::quiet_NaN()), 1.0, 0.0, 6.0) == 1.0);
    CHECK (restoredNumber (juce::var ("nan"), 1.0, 0.0, 6.0) == 1.0);
    CHECK (restoredNumber (juce::var (std::numeric_limits<double>::infinity()), 1.0, 0.0, 6.0) == 1.0);
    CHECK (restoredNumber (juce::var (-std::numeric_limits<double>::infinity()), 1.0, 0.0, 6.0) == 1.0);
    CHECK (restoredNumber (juce::var ("inf"), 1.0, 0.0, 6.0) == 1.0);
    CHECK (restoredNumber (juce::var(), 1.0, 0.0, 6.0) == 0.0); // no value reads as 0, which is in range
}

TEST_CASE ("A state whose Display Range, editor size or Analyzer settings aren't finite numbers restores their defaults; finite ones outside their range, the range's end")
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor maker;
    maker.setDisplayRangeDb (30);
    maker.setEditorSize ({ 1400, 900 });
    auto analyzer = maker.analyzerSettings();
    analyzer.rangeDb = 120;
    analyzer.tiltDbPerOctave = 2.5;
    maker.setAnalyzerSettings (analyzer);
    const auto xml = harness::savedXml (maker);
    REQUIRE (xml != nullptr);
    struct Junk
    {
        const char* text;
        juce::Point<int> editorSize;
        eq1::AnalyzerSpeed speed;
        eq1::AnalyzerResolution resolution;
        double tilt;
    };
    using eq1::AnalyzerSpeed, eq1::AnalyzerResolution;
    using namespace staple::tokens::layout;
    const eq1::AnalyzerSettings defaults;
    // Not finite: the defaults. Finite but outside the range: its end. Text that isn't a number reads as 0.
    const auto junk = GENERATE_COPY (Junk { "nan", { windowWidth, windowHeight }, defaults.speed, defaults.resolution, defaults.tiltDbPerOctave },
                                     Junk { "inf", { windowWidth, windowHeight }, defaults.speed, defaults.resolution, defaults.tiltDbPerOctave },
                                     Junk { "-inf", { windowWidth, windowHeight }, defaults.speed, defaults.resolution, defaults.tiltDbPerOctave },
                                     Junk { "1e30", { maximumWidth, maximumHeight }, AnalyzerSpeed::veryFast, AnalyzerResolution::maximum, 6.0 },
                                     Junk { "-5", { minimumWidth, minimumHeight }, AnalyzerSpeed::verySlow, AnalyzerResolution::low, 0.0 },
                                     Junk { "text", { minimumWidth, minimumHeight }, AnalyzerSpeed::verySlow, AnalyzerResolution::low, 0.0 });
    CAPTURE (junk.text);
    xml->setAttribute ("displayRangeDb", junk.text);
    xml->setAttribute ("editorWidth", junk.text);
    xml->setAttribute ("editorHeight", junk.text);
    auto* saved = xml->getChildByName ("Analyzer");
    REQUIRE (saved != nullptr);
    saved->setAttribute ("rangeDb", junk.text);
    saved->setAttribute ("speed", junk.text);
    saved->setAttribute ("resolution", junk.text);
    saved->setAttribute ("tiltDbPerOctave", junk.text);

    eq1::PluginProcessor restored;
    restored.setDisplayRangeDb (6);
    restored.setEditorSize ({ 1000, 700 });
    harness::restoreState (restored, *xml);
    // The Display Range and the Analyzer's range are one of a few choices: anything else is the default.
    CHECK (restored.displayRangeDb() == 12);
    CHECK (restored.analyzerSettings().rangeDb == defaults.rangeDb);
    CHECK (restored.editorSize() == junk.editorSize);
    CHECK (restored.analyzerSettings().speed == junk.speed);
    CHECK (restored.analyzerSettings().resolution == junk.resolution);
    CHECK (std::isfinite (restored.analyzerSettings().tiltDbPerOctave));
    CHECK (restored.analyzerSettings().tiltDbPerOctave == junk.tilt);
}
