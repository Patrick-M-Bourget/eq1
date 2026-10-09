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

// Attack and Release in %, with their centre shown as Auto.
juce::AudioParameterFloatAttributes timingText()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction ([] (float value, int) { return juce::exactlyEqual (value, 50.0f) ? juce::String ("Auto") : juce::String (value, 1); })
        .withValueFromStringFunction ([] (const juce::String& text) {
            return text.trim().equalsIgnoreCase ("Auto") ? 50.0f : text.getFloatValue();
        });
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
juce::String placementId (int slot) { return slotId (slot, "placement"); }
juce::String dynamicRangeId (int slot) { return slotId (slot, "dynamic_range"); }
juce::String thresholdId (int slot) { return slotId (slot, "threshold"); }
juce::String thresholdAutoId (int slot) { return slotId (slot, "threshold_auto"); }
juce::String attackId (int slot) { return slotId (slot, "attack"); }
juce::String releaseId (int slot) { return slotId (slot, "release"); }
juce::String dynamicsBypassId (int slot) { return slotId (slot, "dynamics_bypass"); }
juce::String detectionSourceId (int slot) { return slotId (slot, "detection_source"); }
juce::String detectionRangeId (int slot) { return slotId (slot, "detection_range"); }
juce::String detectionLowId (int slot) { return slotId (slot, "detection_low"); }
juce::String detectionHighId (int slot) { return slotId (slot, "detection_high"); }

const juce::StringArray& shapeNames()
{
    static const juce::StringArray names { "Bell",      "Low Shelf", "Low Cut",    "High Shelf", "High Cut",
                                           "Notch",     "Band Pass", "Tilt Shelf", "Flat Tilt",  "All Pass" };
    return names;
}

const juce::StringArray& placementNames()
{
    static const juce::StringArray names { "Stereo", "Left", "Right", "Mid", "Side" };
    return names;
}

const juce::StringArray& detectionSourceNames()
{
    static const juce::StringArray names { "Internal", "External" };
    return names;
}

const juce::StringArray& detectionRangeNames()
{
    static const juce::StringArray names { "Band", "Free" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    // Ranges match Pro-Q 4: Frequency 10 Hz to 30 kHz, Gain +/-30 dB, Q 0.025 to 40. Slope is one continuous
    // 0 to 96 dB/oct range shared by every Shape, and Brickwall a separate switch (ADR 0003).
    // Dynamic Range is +/-30 dB as in Pro-Q 4; Threshold -60 to 0 dB, with Auto a separate switch
    // (ADR 0003, Consequences), on by default; Attack and Release 0 to 100%, Auto at 50%. The Free
    // Detection Range's limits span Frequency's range, from 20 Hz to 20 kHz by default.
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
                                                                false),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { placementId (slot), 1 },
                                                                  slotName (slot, "Stereo Placement"),
                                                                  placementNames(),
                                                                  0),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { dynamicRangeId (slot), 1 },
                                                                 slotName (slot, "Dynamic Range"),
                                                                 juce::NormalisableRange<float> (-30.0f, 30.0f),
                                                                 0.0f,
                                                                 withText (2, "dB")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { thresholdId (slot), 1 },
                                                                 slotName (slot, "Threshold"),
                                                                 juce::NormalisableRange<float> (-60.0f, 0.0f),
                                                                 -30.0f,
                                                                 withText (1, "dB")),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { thresholdAutoId (slot), 1 },
                                                                slotName (slot, "Auto Threshold"),
                                                                true),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { attackId (slot), 1 },
                                                                 slotName (slot, "Attack"),
                                                                 juce::NormalisableRange<float> (0.0f, 100.0f),
                                                                 50.0f,
                                                                 timingText()),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { releaseId (slot), 1 },
                                                                 slotName (slot, "Release"),
                                                                 juce::NormalisableRange<float> (0.0f, 100.0f),
                                                                 50.0f,
                                                                 timingText()),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { dynamicsBypassId (slot), 1 },
                                                                slotName (slot, "Dynamics Bypass"),
                                                                false),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { detectionSourceId (slot), 1 },
                                                                  slotName (slot, "Detection Source"),
                                                                  detectionSourceNames(),
                                                                  0),
                    std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { detectionRangeId (slot), 1 },
                                                                  slotName (slot, "Detection Range"),
                                                                  detectionRangeNames(),
                                                                  0),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { detectionLowId (slot), 1 },
                                                                 slotName (slot, "Detection Low"),
                                                                 logRange (10.0f, 30000.0f),
                                                                 20.0f,
                                                                 withText (1, "Hz")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { detectionHighId (slot), 1 },
                                                                 slotName (slot, "Detection High"),
                                                                 logRange (10.0f, 30000.0f),
                                                                 20000.0f,
                                                                 withText (1, "Hz")));
    }
    return layout;
}

SlotValues SlotValues::of (juce::AudioProcessorValueTreeState& parameters, int slot)
{
    return { parameters.getRawParameterValue (frequencyId (slot)), parameters.getRawParameterValue (gainId (slot)),
             parameters.getRawParameterValue (qId (slot)),         parameters.getRawParameterValue (inUseId (slot)),
             parameters.getRawParameterValue (bypassId (slot)),    parameters.getRawParameterValue (shapeId (slot)),
             parameters.getRawParameterValue (slopeId (slot)),     parameters.getRawParameterValue (brickwallId (slot)),
             parameters.getRawParameterValue (placementId (slot)),
             parameters.getRawParameterValue (dynamicRangeId (slot)),
             parameters.getRawParameterValue (thresholdId (slot)),
             parameters.getRawParameterValue (thresholdAutoId (slot)),
             parameters.getRawParameterValue (attackId (slot)),
             parameters.getRawParameterValue (releaseId (slot)),
             parameters.getRawParameterValue (dynamicsBypassId (slot)),
             parameters.getRawParameterValue (detectionSourceId (slot)),
             parameters.getRawParameterValue (detectionRangeId (slot)),
             parameters.getRawParameterValue (detectionLowId (slot)),
             parameters.getRawParameterValue (detectionHighId (slot)) };
}

BandSettings SlotValues::read() const
{
    return { .inUse = inUse->load() >= 0.5f,
             .bypass = bypass->load() >= 0.5f,
             .shape = static_cast<Shape> (juce::roundToInt (shape->load())),
             .frequency = frequency->load(),
             .gain = gain->load(),
             .q = q->load(),
             .slope = slope->load(),
             .brickwall = brickwall->load() >= 0.5f,
             .placement = static_cast<StereoPlacement> (juce::roundToInt (placement->load())),
             .detectionSource = static_cast<DetectionSource> (juce::roundToInt (detectionSource->load())),
             .detectionRange = static_cast<DetectionRange> (juce::roundToInt (detectionRange->load())),
             .detectionLow = detectionLow->load(),
             .detectionHigh = detectionHigh->load(),
             .dynamicRange = dynamicRange->load(),
             .threshold = threshold->load(),
             .thresholdAuto = thresholdAuto->load() >= 0.5f,
             .attack = attack->load(),
             .release = release->load(),
             .dynamicsBypass = dynamicsBypass->load() >= 0.5f };
}

} // namespace eq1::parameters
