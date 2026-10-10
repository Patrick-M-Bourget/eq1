#include "BandEditing.h"
#include "BandPanel.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{

// A plugin with the Band panel the editor shows, and Bands 3 and 4 Bells, 5 a Low Cut.
struct Host
{
    juce::ScopedJuceInitialiser_GUI juce;
    eq1::PluginProcessor processor;
    eq1::BandEditing editing { processor.parameterState(), processor.editHistory() };
    std::unique_ptr<eq1::BandPanel> panel = std::make_unique<eq1::BandPanel> (processor, editing);

    Host()
    {
        for (int slot : { 3, 4, 5 })
            set (slot, "in_use", 1.0f);
        set (5, "shape", 2.0f);
    }

    void set (int slot, const char* control, float plain)
    {
        auto* parameter = processor.parameterState().getParameter ("band" + juce::String (slot) + "_" + control);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }
};

} // namespace

TEST_CASE ("The Band panel meters the Band it shows, when its Shape has dynamics")
{
    Host host;
    CHECK (host.processor.meteredSlot() == 0);

    host.panel->show (3);
    CHECK (host.processor.meteredSlot() == 3);

    // Selecting another Band moves the metering to it.
    host.panel->show (4);
    CHECK (host.processor.meteredSlot() == 4);

    SECTION ("showing no Band lets go of it")
    {
        host.panel->show (0);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("a Shape without dynamics isn't metered")
    {
        host.panel->show (5);
        CHECK (host.processor.meteredSlot() == 0);
    }
    SECTION ("closing the editor lets go of it")
    {
        host.panel.reset();
        CHECK (host.processor.meteredSlot() == 0);
    }
}
