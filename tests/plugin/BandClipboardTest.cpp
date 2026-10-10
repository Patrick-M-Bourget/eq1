#include "BandClipboard.h"
#include "BandEditing.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinRel;

using eq1::BandSettings;

namespace
{

// A Band with every stored setting off its default, each a value a parameter holds exactly.
BandSettings everySettingChanged()
{
    return { .inUse = true,
             .bypass = true,
             .shape = eq1::Shape::HighShelf,
             .frequency = 2500.0,
             .gain = -7.5,
             .q = 2.5,
             .slope = 36.0,
             .brickwall = true,
             .placement = eq1::StereoPlacement::Side,
             .detectionSource = eq1::DetectionSource::External,
             .detectionRange = eq1::DetectionRange::Free,
             .detectionLow = 300.0,
             .detectionHigh = 6000.0,
             .dynamicRange = 12.5,
             .threshold = -42.0,
             .thresholdAuto = false,
             .attack = 20.0,
             .release = 80.0,
             .dynamicsBypass = true };
}

// A plugin as a host has it, with the editing rules the editor uses on top, at a Gain Scale in %.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };

    explicit Host (float gainScale)
    {
        auto* p = processor.parameterState().getParameter (eq1::parameters::gainScaleId);
        p->setValueNotifyingHost (p->convertTo0to1 (gainScale));
    }

    float value (const juce::String& id) { return processor.parameterState().getRawParameterValue (id)->load(); }
};

BandSettings bandAt (double frequency)
{
    BandSettings band;
    band.inUse = true;
    band.frequency = frequency;
    return band;
}

} // namespace

TEST_CASE ("Copied Bands read back from the clipboard text with every stored setting, in order")
{
    const std::vector<BandSettings> bands { everySettingChanged(), bandAt (80.0) };
    const auto text = eq1::captureBands (bands).toXmlString();
    CHECK (eq1::clipboardBands (text) == bands);
}

TEST_CASE ("The clipboard holds each Band's settings in the Preset form, relative to the Band, stamped with the state version")
{
    const auto tree = eq1::captureBands ({ bandAt (80.0) });
    CHECK (tree.getType().toString() == "eq1Bands");
    CHECK (static_cast<int> (tree.getProperty ("version")) == eq1::PluginProcessor::stateVersion);
    REQUIRE (tree.getNumChildren() == 1);
    const auto band = tree.getChild (0);
    CHECK (band.hasType ("Band"));
    CHECK (static_cast<double> (band.getChildWithProperty ("id", "frequency").getProperty ("value")) == 80.0);
    for (const auto& setting : band)
    {
        CHECK (setting.hasType ("PARAM"));
        CHECK_FALSE (setting["id"].toString().startsWith ("band"));
        CHECK (setting["id"].toString() != "in_use");
    }
}

TEST_CASE ("Clipboard content that isn't eq1's Bands holds no Bands")
{
    CHECK (eq1::clipboardBands ({}).empty());
    CHECK (eq1::clipboardBands ("some text").empty());
    CHECK (eq1::clipboardBands ("<eq1 version=\"3\"><PARAM id=\"band1_in_use\" value=\"1\"/></eq1>").empty());
    CHECK (eq1::clipboardBands ("<eq1Bands version=\"3\"/>").empty());
}

TEST_CASE ("Bands from a newer eq1 read what this one knows; a setting a Band leaves out is at its default")
{
    const auto text = juce::String ("<eq1Bands version=\"99\">"
                                    "<Band><PARAM id=\"frequency\" value=\"500\"/><PARAM id=\"sparkle\" value=\"3\"/></Band>"
                                    "<Band><PARAM id=\"gain\" value=\"-6\"/><Future/></Band>"
                                    "</eq1Bands>");
    auto first = bandAt (500.0);
    auto second = bandAt (1000.0);
    second.gain = -6.0;
    CHECK (eq1::clipboardBands (text) == std::vector<BandSettings> { first, second });
}

TEST_CASE ("Copy then Paste in another instance recreates the Bands with every stored setting, whatever either Gain Scale")
{
    Host from (50.0f), to (200.0f);
    REQUIRE (from.editing.paste ({ everySettingChanged(), bandAt (80.0) }) == std::vector<int> { 1, 2 });
    to.editing.add (100.0, 3.0);

    const auto text = eq1::captureBands ({ from.editing.band (1), from.editing.band (2) }).toXmlString();
    CHECK (to.editing.paste (eq1::clipboardBands (text)) == std::vector<int> { 2, 3 });
    for (const auto& control : { "bypass", "shape", "frequency", "gain", "q", "slope", "brickwall", "placement", "dynamic_range", "threshold",
                                 "threshold_auto", "attack", "release", "dynamics_bypass", "detection_source", "detection_range",
                                 "detection_low", "detection_high", "in_use" })
    {
        INFO (control);
        CHECK_THAT (to.value ("band2_" + juce::String (control)), WithinRel (from.value ("band1_" + juce::String (control)), 1.0e-5f));
        CHECK_THAT (to.value ("band3_" + juce::String (control)), WithinRel (from.value ("band2_" + juce::String (control)), 1.0e-5f));
    }
}
