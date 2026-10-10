#pragma once

#include "BandEditing.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace eq1
{

// The EQ display's context menu. On a selection of Bands: Bypass (Remove Bypass when every one is
// Bypassed), Invert Gain, Clear Dynamics; the Shape, Slope and Stereo Placement submenus; Cut, Copy,
// Paste and Split; Delete; Select All. On empty space, Paste and Select All. Each item applies to
// every selected Band it can, as one edit, and is unavailable when it applies to none. A submenu
// item is ticked only when every Band it applies to has that value. Split needs a stereo track and a
// free Band Slot, and reads "Split (N of M)" when there are free slots for only N of the M selected
// Stereo Bands. Copy puts the selected Bands on the clipboard (BandClipboard.h), as no edit; Cut
// also deletes them. Paste adds the clipboard's Bands and selects them; it needs eq1's Bands on the
// clipboard and a free Band Slot, and reads "Paste (N of M)" as Split does.
// The Slope list, shared by the Band menu's Slope submenu and the Band panel's Slope button: 6, 12,
// 18, 24, 30, 36, 48, 72 and 96 dB/oct, then Brickwall on Cuts. Each item sets the given Bands, as one
// edit, and is ticked as the Band menu ticks. The items keep editing by reference.
juce::PopupMenu slopeMenu (BandEditing& editing, const std::vector<int>& slots);

struct BandMenu
{
    BandEditing& editing;
    std::vector<int> selection; // empty on empty space
    bool stereoPlacementAvailable;
    std::function<void()> deleteSelection, selectAll;
    std::function<void (std::vector<int>)> select; // the slots to select after Split or Paste
    juce::String clipboard; // the system clipboard's text
    std::function<void (const juce::String&)> copyToClipboard;

    juce::PopupMenu build() const;
};

} // namespace eq1
