#include "Parameters.h"

#include "eq1/Settings.h"

#include <cmath>
#include <limits>

namespace eq1::parameters
{

namespace
{

juce::String slotId (int slot, const char* control) { return "band" + juce::String (slot) + "_" + control; }
juce::String slotName (int slot, const char* control) { return "Band " + juce::String (slot) + " " + control; }

// A range that moves evenly in log space, as Frequency and Q are heard. The maths runs in double
// so a value set by the host survives save and reload exactly.
juce::NormalisableRange<float> logRange (ParameterRange range)
{
    return { static_cast<float> (range.low),
             static_cast<float> (range.high),
             [] (float start, float end, float normalised) {
                 return static_cast<float> (start * std::pow (double { end } / start, double { normalised }));
             },
             [] (float start, float end, float value) {
                 return static_cast<float> (std::log (double { value } / start) / std::log (double { end } / start));
             },
             [] (float, float, float value) { return value; } };
}

// A range that moves evenly.
juce::NormalisableRange<float> linearRange (ParameterRange range)
{
    return { static_cast<float> (range.low), static_cast<float> (range.high) };
}

// Output Gain: -inf (its silent bottom) to +36 dB, skewed so 0 dB sits at the centre. The skew's float maths
// misses 0 dB by a few float steps (the centre reads 1.4e-6 dB), so anything that close is exactly 0 dB:
// the default passes audio through bit for bit.
juce::NormalisableRange<float> outputGainRange()
{
    constexpr float snapToZeroDb = 1.0e-4f; // about 20 float steps either side of the centre
    juce::NormalisableRange<float> skewed = linearRange (parameterRanges::outputGain);
    skewed.setSkewForCentre (0.0f);
    return { skewed.start,
             skewed.end,
             [skewed] (float, float, float normalised) {
                 const float value = skewed.convertFrom0to1 (normalised);
                 return std::abs (value) < snapToZeroDb ? 0.0f : value;
             },
             [skewed] (float, float, float value) { return value == 0.0f ? 0.5f : skewed.convertTo0to1 (value); } };
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

const juce::StringArray& panModeNames()
{
    static const juce::StringArray names { "L/R", "M/S" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    // The ranges are Pro-Q 4's (eq1/Settings.h, parameterRanges). Slope is one continuous range shared by
    // every Shape, and Brickwall a separate switch (ADR 0003). Threshold's Auto is a separate switch
    // (ADR 0003, Consequences), on by default; Attack and Release are Auto at 50%. The Free Detection
    // Range's limits span Frequency's range, from 20 Hz to 20 kHz by default.
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { frequencyId (slot), 1 },
                                                                 slotName (slot, "Frequency"),
                                                                 logRange (parameterRanges::frequency),
                                                                 1000.0f,
                                                                 withText (1, "Hz")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainId (slot), 1 },
                                                                 slotName (slot, "Gain"),
                                                                 linearRange (parameterRanges::gain),
                                                                 0.0f,
                                                                 withText (2, "dB")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { qId (slot), 1 },
                                                                 slotName (slot, "Q"),
                                                                 logRange (parameterRanges::q),
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
                                                                 linearRange (parameterRanges::slope),
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
                                                                 linearRange (parameterRanges::dynamicRange),
                                                                 0.0f,
                                                                 withText (2, "dB")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { thresholdId (slot), 1 },
                                                                 slotName (slot, "Threshold"),
                                                                 linearRange (parameterRanges::threshold),
                                                                 -30.0f,
                                                                 withText (1, "dB")),
                    std::make_unique<juce::AudioParameterBool> (juce::ParameterID { thresholdAutoId (slot), 1 },
                                                                slotName (slot, "Auto Threshold"),
                                                                true),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { attackId (slot), 1 },
                                                                 slotName (slot, "Attack"),
                                                                 linearRange (parameterRanges::timing),
                                                                 50.0f,
                                                                 timingText()),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { releaseId (slot), 1 },
                                                                 slotName (slot, "Release"),
                                                                 linearRange (parameterRanges::timing),
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
                                                                 logRange (parameterRanges::frequency),
                                                                 20.0f,
                                                                 withText (1, "Hz")),
                    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { detectionHighId (slot), 1 },
                                                                 slotName (slot, "Detection High"),
                                                                 logRange (parameterRanges::frequency),
                                                                 20000.0f,
                                                                 withText (1, "Hz")));
    }

    // Output Gain has 0 dB at the centre and its bottom silent; Output Pan goes from left (or Mid) to
    // right (or Side).
    const auto outputGainText = juce::AudioParameterFloatAttributes()
                                    .withLabel ("dB")
                                    .withStringFromValueFunction ([] (float value, int) {
                                        return value <= outputGainSilentDb ? juce::String ("-inf") : juce::String (value, 2);
                                    })
                                    .withValueFromStringFunction ([] (const juce::String& text) {
                                        return text.trim().equalsIgnoreCase ("-inf") ? outputGainSilentDb : text.getFloatValue();
                                    });
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainScaleId, 1 },
                                                             "Gain Scale",
                                                             linearRange (parameterRanges::gainScale),
                                                             100.0f,
                                                             withText (1, "%")),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoGainId, 1 }, "Auto Gain", false),
                std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { outputGainId, 1 }, "Output Gain", outputGainRange(), 0.0f, outputGainText),
                std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { outputPanId, 1 },
                                                             "Output Pan",
                                                             linearRange (parameterRanges::outputPan),
                                                             0.0f,
                                                             withText (1, "%")),
                std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { panModeId, 1 }, "Pan Mode", panModeNames(), 0),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { phaseInvertId, 1 }, "Phase Invert", false),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { globalBypassId, 1 }, "Global Bypass", false));
    return layout;
}

void setTogether (const std::vector<NewValue>& values)
{
    for (const auto& [parameter, value] : values)
        parameter->beginChangeGesture();
    for (const auto& [parameter, value] : values)
        parameter->setValueNotifyingHost (value);
    for (const auto& [parameter, value] : values)
        parameter->endChangeGesture();
}

juce::RangedAudioParameter& lookUp (juce::AudioProcessorValueTreeState& parameters, const juce::String& id)
{
    if (auto* found = parameters.getParameter (id))
        return *found;
    jassertfalse; // an id the layout doesn't have
    static juce::AudioParameterFloat inert { juce::ParameterID { "inert", 1 }, {}, juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f };
    return inert;
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
    return { .inUse = isOn (inUse),
             .bypass = isOn (bypass),
             .shape = static_cast<Shape> (juce::roundToInt (shape->load())),
             .frequency = frequency->load(),
             .gain = gain->load(),
             .q = q->load(),
             .slope = slope->load(),
             .brickwall = isOn (brickwall),
             .placement = static_cast<StereoPlacement> (juce::roundToInt (placement->load())),
             .detectionSource = static_cast<DetectionSource> (juce::roundToInt (detectionSource->load())),
             .detectionRange = static_cast<DetectionRange> (juce::roundToInt (detectionRange->load())),
             .detectionLow = detectionLow->load(),
             .detectionHigh = detectionHigh->load(),
             .dynamicRange = dynamicRange->load(),
             .threshold = threshold->load(),
             .thresholdAuto = isOn (thresholdAuto),
             .attack = attack->load(),
             .release = release->load(),
             .dynamicsBypass = isOn (dynamicsBypass) };
}

OutputValues OutputValues::of (juce::AudioProcessorValueTreeState& parameters)
{
    return { parameters.getRawParameterValue (gainScaleId),   parameters.getRawParameterValue (autoGainId),
             parameters.getRawParameterValue (outputGainId),  parameters.getRawParameterValue (outputPanId),
             parameters.getRawParameterValue (panModeId),     parameters.getRawParameterValue (phaseInvertId),
             parameters.getRawParameterValue (globalBypassId) };
}

void OutputValues::readInto (Settings& settings) const
{
    const float outputGainDb = outputGain->load();
    settings.gainScale = gainScale->load() / 100.0;
    settings.autoGain = isOn (autoGain);
    settings.outputGainDb = outputGainDb <= outputGainSilentDb ? -std::numeric_limits<double>::infinity() : outputGainDb;
    settings.outputPan = outputPan->load() / 100.0;
    settings.panMode = static_cast<PanMode> (juce::roundToInt (panMode->load()));
    settings.phaseInvert = isOn (phaseInvert);
    settings.globalBypass = isOn (globalBypass);
}

SettingsReader::SettingsReader (juce::AudioProcessorValueTreeState& parameters) : outputs (OutputValues::of (parameters))
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slots[static_cast<size_t> (slot - 1)] = SlotValues::of (parameters, slot);
}

Settings SettingsReader::read() const
{
    Settings settings;
    for (size_t i = 0; i < slots.size(); ++i)
        settings.bands[i] = slots[i].read();
    outputs.readInto (settings);
    return settings;
}

} // namespace eq1::parameters
