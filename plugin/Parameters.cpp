#include "Parameters.h"

#include "eq1/Settings.h"

#include <cmath>

namespace eq1::parameters
{

namespace
{

juce::String slotId (int slot, const char* control) { return "band" + juce::String (slot) + "_" + control; }
juce::String slotName (int slot, const char* control) { return "Band " + juce::String (slot) + " " + control; }

// A range that moves evenly in log space, as Frequency and Q are heard. The maths runs in double
// so a value set by the host survives save and reload exactly.
juce::NormalisableRange<float> logRange (float minimum, float maximum)
{
    return { minimum,
             maximum,
             [] (float start, float end, float normalised) {
                 return static_cast<float> (start * std::pow (double { end } / start, double { normalised }));
             },
             [] (float start, float end, float value) {
                 return static_cast<float> (std::log (double { value } / start) / std::log (double { end } / start));
             },
             [] (float, float, float value) { return value; } };
}

// Fixed-precision text, so value -> text -> value -> text round-trips.
juce::AudioParameterFloatAttributes withText (int decimals, const juce::String& label = {})
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (label)
        .withStringFromValueFunction ([decimals] (float value, int) { return juce::String (value, decimals); })
        .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); });
}

} // namespace

juce::String frequencyId (int slot) { return slotId (slot, "frequency"); }
juce::String gainId (int slot) { return slotId (slot, "gain"); }
juce::String qId (int slot) { return slotId (slot, "q"); }
juce::String inUseId (int slot) { return slotId (slot, "in_use"); }
juce::String bypassId (int slot) { return slotId (slot, "bypass"); }
juce::String shapeId (int slot) { return slotId (slot, "shape"); }
juce::String slopeId (int slot) { return slotId (slot, "slope"); }
juce::String brickwallId (int slot) { return slotId (slot, "brickwall"); }

const juce::StringArray& shapeNames()
{
    static const juce::StringArray names { "Bell",      "Low Shelf", "Low Cut",    "High Shelf", "High Cut",
                                           "Notch",     "Band Pass", "Tilt Shelf", "Flat Tilt",  "All Pass" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    // Ranges match Pro-Q 4: Frequency 10 Hz to 30 kHz, Gain +/-30 dB, Q 0.025 to 40. Slope is one continuous
    // 0 to 96 dB/oct range shared by every Shape, and Brickwall a separate switch (ADR 0003).
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { frequencyId (slot), 1 },
                                                                 slotName (slot, "Frequency"),
                                                                 logRange (10.0f, 30000.0f),
                                                                 1000.0f,
                                                                 withText (1, "Hz")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainId (slot), 1 },
                                                                 slotName (slot, "Gain"),
                                                                 juce::NormalisableRange<float> (-30.0f, 30.0f),
                                                                 0.0f,
                                                                 withText (2, "dB")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { qId (slot), 1 },
                                                                 slotName (slot, "Q"),
                                                                 logRange (0.025f, 40.0f),
                                                                 1.0f,
                                                                 withText (3)),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { inUseId (slot), 1 },
                                                                slotName (slot, "In Use"),
                                                                false),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypassId (slot), 1 },
                                                                slotName (slot, "Bypass"),
                                                                false),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { shapeId (slot), 1 },
                                                                  slotName (slot, "Shape"),
                                                                  shapeNames(),
                                                                  0),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slopeId (slot), 1 },
                                                                 slotName (slot, "Slope"),
                                                                 juce::NormalisableRange<float> (0.0f, 96.0f),
                                                                 12.0f,
                                                                 withText (1, "dB/oct")),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { brickwallId (slot), 1 },
                                                                slotName (slot, "Brickwall"),
                                                                false));
    }
    return layout;
}

} // namespace eq1::parameters
