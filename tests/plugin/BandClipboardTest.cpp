#include "BandClipboard.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

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
