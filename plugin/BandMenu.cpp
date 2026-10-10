#include "BandMenu.h"

#include "BandClipboard.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace eq1
{

namespace
{
// The Slope list's values, in dB/oct; Brickwall follows them.
constexpr std::array<double, 9> listedSlopes { 6.0, 12.0, 18.0, 24.0, 30.0, 36.0, 48.0, 72.0, 96.0 };

// Off a default by more than its parameter's round trip through a normalised value: the Free limits'
// log ranges don't give their defaults back exactly.
bool differs (double value, double defaultValue) { return std::abs (value - defaultValue) > 1.0e-5 * std::max (1.0, std::abs (defaultValue)); }

// A Shape with dynamics whose every dynamics setting is already its default has nothing to clear.
bool hasDynamicsToClear (const BandSettings& band)
{
    const BandSettings defaults;
    return hasDynamics (band.shape)
           && (differs (band.dynamicRange, defaults.dynamicRange) || differs (band.threshold, defaults.threshold)
               || band.thresholdAuto != defaults.thresholdAuto || differs (band.attack, defaults.attack)
               || differs (band.release, defaults.release) || band.dynamicsBypass != defaults.dynamicsBypass
               || band.detectionSource != defaults.detectionSource || band.detectionRange != defaults.detectionRange
               || differs (band.detectionLow, defaults.detectionLow) || differs (band.detectionHigh, defaults.detectionHigh));
}

bool isBrickwall (const BandSettings& band) { return isCut (band.shape) && band.brickwall; }
} // namespace

juce::String slopeText (double slope, bool brickwall)
{
    if (brickwall)
        return "Brickwall";
    const bool whole = std::abs (slope - std::round (slope)) < 0.05;
    return juce::String (slope, whole ? 0 : 1) + " dB/oct";
}

juce::PopupMenu slopeMenu (BandEditing& edit, const std::vector<int>& slots)
{
    std::vector<BandSettings> bands;
    for (int slot : slots)
        bands.push_back (edit.band (slot));
    const auto any = [&] (auto&& predicate) { return std::any_of (bands.begin(), bands.end(), predicate); };
    // Ticked when at least one Band is one the item applies to, and every such Band has its value.
    const auto shared = [&] (auto&& appliesTo, auto&& hasValue) {
        return any (appliesTo) && std::all_of (bands.begin(), bands.end(), [&] (const BandSettings& b) { return ! appliesTo (b) || hasValue (b); });
    };
    // The Slope parameter is continuous: a listed value is ticked only where every Band exactly equals it.
    const auto withSlope = [] (const BandSettings& b) { return hasSlope (b.shape); };
    const auto cut = [] (const BandSettings& b) { return isCut (b.shape); };
    juce::PopupMenu slopes;
    for (double slope : listedSlopes)
        slopes.addItem (juce::String (juce::roundToInt (slope)) + " dB/oct",
                        true,
                        shared (withSlope, [slope] (const BandSettings& b) { return ! isBrickwall (b) && juce::exactlyEqual (b.slope, slope); }),
                        [&edit, slots, slope] { edit.setSlope (slots, slope); });
    slopes.addItem ("Brickwall", any (cut), shared (cut, isBrickwall), [&edit, slots] { edit.setBrickwall (slots); });
    return slopes;
}

juce::PopupMenu BandMenu::build() const
{
    juce::PopupMenu menu;
    std::vector<BandSettings> bands;
    for (int slot : selection)
        bands.push_back (editing.band (slot));
    const auto any = [&] (auto&& predicate) { return std::any_of (bands.begin(), bands.end(), predicate); };
    // Ticked when at least one selected Band is one the item applies to, and every such Band has its value.
    const auto shared = [&] (auto&& appliesTo, auto&& hasValue) {
        return any (appliesTo) && std::all_of (bands.begin(), bands.end(), [&] (const BandSettings& b) { return ! appliesTo (b) || hasValue (b); });
    };
    const auto all = [] (const BandSettings&) { return true; };
    // The actions keep the selection by copy and BandEditing by reference: they outlive this BandMenu.
    auto& edit = editing;
    const auto slots = selection;

    // Paste, with the Bands on the clipboard that fit into the free Band Slots.
    const auto pasted = clipboardBands (clipboard);
    const auto toPaste = static_cast<int> (pasted.size());
    const int pastes = std::min (toPaste, editing.freeSlots());
    const auto addPaste = [&] {
        menu.addItem (pastes > 0 && pastes < toPaste ? "Paste (" + juce::String (pastes) + " of " + juce::String (toPaste) + ")" : juce::String ("Paste"),
                      pastes > 0,
                      false,
                      [&edit, pasted, selectPasted = select] { selectPasted (edit.paste (pasted)); });
    };

    if (! bands.empty())
    {
        const bool allBypassed = ! any ([] (const BandSettings& b) { return ! b.bypass; });
        menu.addItem (allBypassed ? "Remove Bypass" : "Bypass", [&edit, slots, allBypassed] { edit.setBypass (slots, ! allBypassed); });
        menu.addItem ("Invert Gain", any ([] (const BandSettings& b) { return hasGain (b.shape); }), false, [&edit, slots] {
            edit.invertGain (slots);
        });
        menu.addItem ("Clear Dynamics", any (hasDynamicsToClear), false, [&edit, slots] { edit.clearDynamics (slots); });

        menu.addSeparator();
        juce::PopupMenu shapes;
        for (int i = 0; i < parameters::shapeNames().size(); ++i)
        {
            const auto shape = static_cast<Shape> (i);
            shapes.addItem (parameters::shapeNames()[i], true, shared (all, [shape] (const BandSettings& b) { return b.shape == shape; }),
                            [&edit, slots, shape] { edit.setShape (slots, shape); });
        }
        menu.addSubMenu ("Shape", shapes);

        menu.addSubMenu ("Slope", slopeMenu (editing, selection), any ([] (const BandSettings& b) { return hasSlope (b.shape); }));

        juce::PopupMenu placements;
        for (int i = 0; i < parameters::placementNames().size(); ++i)
        {
            const auto placement = static_cast<StereoPlacement> (i);
            placements.addItem (parameters::placementNames()[i],
                                true,
                                shared (all, [placement] (const BandSettings& b) { return b.placement == placement; }),
                                [&edit, slots, placement] { edit.setPlacement (slots, placement); });
        }
        menu.addSubMenu ("Stereo Placement", placements, stereoPlacementAvailable);

        menu.addSeparator();
        const auto copy = [&edit, slots, toClipboard = copyToClipboard] {
            std::vector<BandSettings> copied;
            for (int slot : slots)
                copied.push_back (edit.band (slot));
            toClipboard (captureBands (copied).toXmlString());
        };
        menu.addItem ("Cut", [copy, deleteBands = deleteSelection] {
            copy();
            deleteBands();
        });
        menu.addItem ("Copy", copy);
        addPaste();
        const auto toSplit = static_cast<int> (std::count_if (bands.begin(), bands.end(), [] (const BandSettings& b) {
            return b.placement == StereoPlacement::Stereo;
        }));
        const int splits = std::min (toSplit, editing.freeSlots());
        menu.addItem (splits > 0 && splits < toSplit ? "Split (" + juce::String (splits) + " of " + juce::String (toSplit) + ")" : juce::String ("Split"),
                      stereoPlacementAvailable && splits > 0,
                      false,
                      [&edit, slots, selectHalves = select] { selectHalves (edit.split (slots)); });
        menu.addSeparator();
        menu.addItem ("Delete", deleteSelection);
        menu.addSeparator();
    }
    else
    {
        addPaste();
        menu.addSeparator();
    }
    menu.addItem ("Select All", selectAll);
    return menu;
}

} // namespace eq1
