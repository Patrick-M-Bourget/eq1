#include "BandClipboard.h"

#include "Parameters.h"
#include "PluginProcessor.h"

#include <array>

namespace eq1
{

namespace
{
const juce::Identifier clipboardType { "eq1Bands" }, bandType { "Band" }, parameterType { "PARAM" }, idProperty { "id" },
    valueProperty { "value" }, versionProperty { "version" };

// A stored setting of a Band: the parameter it is, and its plain value in BandSettings.
struct Setting
{
    juce::String (*idOf) (int slot);
    double (*get) (const BandSettings&);
    void (*set) (BandSettings&, double);
};

template <typename Choice>
Choice choiceOf (double value, const juce::StringArray& names)
{
    return static_cast<Choice> (juce::jlimit (0, names.size() - 1, juce::roundToInt (value)));
}

// Every setting a Band stores but In Use.
const std::array<Setting, 18> settings { {
    { parameters::bypassId, [] (const BandSettings& b) { return b.bypass ? 1.0 : 0.0; }, [] (BandSettings& b, double v) { b.bypass = v >= 0.5; } },
    { parameters::shapeId,
      [] (const BandSettings& b) { return static_cast<double> (b.shape); },
      [] (BandSettings& b, double v) { b.shape = choiceOf<Shape> (v, parameters::shapeNames()); } },
    { parameters::frequencyId, [] (const BandSettings& b) { return b.frequency; }, [] (BandSettings& b, double v) { b.frequency = v; } },
    { parameters::gainId, [] (const BandSettings& b) { return b.gain; }, [] (BandSettings& b, double v) { b.gain = v; } },
    { parameters::qId, [] (const BandSettings& b) { return b.q; }, [] (BandSettings& b, double v) { b.q = v; } },
    { parameters::slopeId, [] (const BandSettings& b) { return b.slope; }, [] (BandSettings& b, double v) { b.slope = v; } },
    { parameters::brickwallId, [] (const BandSettings& b) { return b.brickwall ? 1.0 : 0.0; }, [] (BandSettings& b, double v) { b.brickwall = v >= 0.5; } },
    { parameters::placementId,
      [] (const BandSettings& b) { return static_cast<double> (b.placement); },
      [] (BandSettings& b, double v) { b.placement = choiceOf<StereoPlacement> (v, parameters::placementNames()); } },
    { parameters::dynamicRangeId, [] (const BandSettings& b) { return b.dynamicRange; }, [] (BandSettings& b, double v) { b.dynamicRange = v; } },
    { parameters::thresholdId, [] (const BandSettings& b) { return b.threshold; }, [] (BandSettings& b, double v) { b.threshold = v; } },
    { parameters::thresholdAutoId,
      [] (const BandSettings& b) { return b.thresholdAuto ? 1.0 : 0.0; },
      [] (BandSettings& b, double v) { b.thresholdAuto = v >= 0.5; } },
    { parameters::attackId, [] (const BandSettings& b) { return b.attack; }, [] (BandSettings& b, double v) { b.attack = v; } },
    { parameters::releaseId, [] (const BandSettings& b) { return b.release; }, [] (BandSettings& b, double v) { b.release = v; } },
    { parameters::dynamicsBypassId,
      [] (const BandSettings& b) { return b.dynamicsBypass ? 1.0 : 0.0; },
      [] (BandSettings& b, double v) { b.dynamicsBypass = v >= 0.5; } },
    { parameters::detectionSourceId,
      [] (const BandSettings& b) { return static_cast<double> (b.detectionSource); },
      [] (BandSettings& b, double v) { b.detectionSource = choiceOf<DetectionSource> (v, parameters::detectionSourceNames()); } },
    { parameters::detectionRangeId,
      [] (const BandSettings& b) { return static_cast<double> (b.detectionRange); },
      [] (BandSettings& b, double v) { b.detectionRange = choiceOf<DetectionRange> (v, parameters::detectionRangeNames()); } },
    { parameters::detectionLowId, [] (const BandSettings& b) { return b.detectionLow; }, [] (BandSettings& b, double v) { b.detectionLow = v; } },
    { parameters::detectionHighId, [] (const BandSettings& b) { return b.detectionHigh; }, [] (BandSettings& b, double v) { b.detectionHigh = v; } },
} };

// A setting's id relative to the Band: band1_frequency's is frequency.
juce::String relativeId (const Setting& setting) { return setting.idOf (1).fromFirstOccurrenceOf ("_", false, false); }

// Brings Bands from an older version of the format up to PluginProcessor::stateVersion, as the state's
// migration does a Preset. No version has yet changed what a Band holds; the clipboard began at 3.
void migrate (juce::ValueTree& tree)
{
    static_assert (PluginProcessor::stateVersion == 3, "A new state version may change what a Band holds: add its step here");
    if (static_cast<int> (tree.getProperty (versionProperty, 0)) < 3)
        tree.setProperty (versionProperty, 3, nullptr);
}
} // namespace

juce::ValueTree captureBands (const std::vector<BandSettings>& bands)
{
    juce::ValueTree tree (clipboardType);
    tree.setProperty (versionProperty, PluginProcessor::stateVersion, nullptr);
    for (const auto& band : bands)
    {
        juce::ValueTree child (bandType);
        for (const auto& setting : settings)
            child.appendChild (juce::ValueTree (parameterType)
                                   .setProperty (idProperty, relativeId (setting), nullptr)
                                   .setProperty (valueProperty, setting.get (band), nullptr),
                               nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

std::vector<BandSettings> readBands (const juce::ValueTree& tree)
{
    if (! tree.hasType (clipboardType))
        return {};
    auto upToDate = tree.createCopy();
    migrate (upToDate);
    std::vector<BandSettings> bands;
    for (const auto& child : upToDate)
    {
        if (! child.hasType (bandType))
            continue;
        BandSettings band;
        band.inUse = true;
        for (const auto& setting : settings)
            if (const auto saved = child.getChildWithProperty (idProperty, relativeId (setting)); saved.hasType (parameterType))
                setting.set (band, saved.getProperty (valueProperty));
        bands.push_back (band);
    }
    return bands;
}

std::vector<BandSettings> clipboardBands (const juce::String& text)
{
    const auto xml = juce::parseXML (text);
    return xml != nullptr ? readBands (juce::ValueTree::fromXml (*xml)) : std::vector<BandSettings> {};
}

} // namespace eq1
