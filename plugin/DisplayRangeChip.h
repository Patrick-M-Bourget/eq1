#pragma once

#include "EditorTick.h"
#include "staple/controls/TextChip.h"

namespace eq1
{

class PluginProcessor;

// The Display Range chip at the EQ display's top right (HANDOFF.md §4): a filled TextChip reading
// "±12 dB" with a chevron, whose menu offers +/-6, 12 and 30 dB with the current one ticked. It
// follows the range wherever it changes (a restored session, auto-zoom) on the editor's tick
// (EditorTick). A screen reader reads it as "Display Range" with the range as its value.
class DisplayRangeChip final : public staple::TextChip, private EditorTick::Client
{
public:
    explicit DisplayRangeChip (PluginProcessor& processor);

    // The menu a click opens.
    juce::PopupMenu menu();

    void clicked() override;

private:
    void tick (double seconds) override { follow(); }
    // Shows the processor's Display Range.
    void follow();

    PluginProcessor& processor;
};

} // namespace eq1
