#pragma once

#include "eq1/Settings.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace eq1
{

// Cut, Copy and Paste carry Bands between eq1 instances as text on the system clipboard: XML with
// an eq1Bands root stamped with the state version, and a Band child per Band holding its stored
// settings as PARAM children, as a Preset does, but with ids relative to the Band ("frequency", not
// "band3_frequency"). In Use isn't held: every Band on the clipboard is one.

juce::ValueTree captureBands (const std::vector<BandSettings>& bands);

// The Bands in tree, in order, brought up to date as a Preset is. A setting a Band leaves out is at
// its default, and one from a newer eq1 that this one doesn't know is ignored. None for a tree that
// isn't eq1's Bands.
std::vector<BandSettings> readBands (const juce::ValueTree& tree);

// The same from the clipboard's text: none for text that isn't eq1's.
std::vector<BandSettings> clipboardBands (const juce::String& text);

} // namespace eq1
