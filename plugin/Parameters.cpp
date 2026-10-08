#include "Parameters.h"

#include <cmath>

namespace eq1::parameters
{

namespace
{

// A range that moves evenly in log space, as Frequency and Q are heard.
juce::NormalisableRange<float> logRange (float minimum, float maximum)
{
    return { minimum,
             maximum,
             [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
             [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); },
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

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    // Ranges match Pro-Q 4: Frequency 10 Hz to 30 kHz, Gain +/-30 dB, Q 0.025 to 40.
    return {
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { band1Frequency, 1 },
                                                     "Band 1 Frequency",
                                                     logRange (10.0f, 30000.0f),
                                                     1000.0f,
                                                     withText (1, "Hz")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { band1Gain, 1 },
                                                     "Band 1 Gain",
                                                     juce::NormalisableRange<float> (-30.0f, 30.0f),
                                                     0.0f,
                                                     withText (2, "dB")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { band1Q, 1 },
                                                     "Band 1 Q",
                                                     logRange (0.025f, 40.0f),
                                                     1.0f,
                                                     withText (3)),
    };
}

} // namespace eq1::parameters
