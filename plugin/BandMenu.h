#pragma once

#include "BandEditing.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace eq1
{

// The EQ display's context menu. On a selection of Bands: Bypass (Remove Bypass when every one is
// Bypassed), Invert Gain, Clear Dynamics; the Shape, Slope and Stereo Placement submenus; Delete;
// Select All. On empty space, Select All only. Each item applies to every selected Band it can, as
// one edit, and is unavailable when it applies to none. A submenu item is ticked only when every
// Band it applies to has that value.
struct BandMenu
{
    BandEditing& editing;
    std::vector<int> selection; // empty on empty space
    bool stereoPlacementAvailable;
    std::function<void()> deleteSelection, selectAll;

    juce::PopupMenu build() const;
};

} // namespace eq1
