#pragma once

#include <juce_core/juce_core.h>

// Values as the editor writes them: Frequency, signed dB, Slope and percent, in one place. Every display
// and readout uses these, and so does the accessibility text built from them. A parameter's own text,
// which hosts show and save, is the parameter's (Parameters.cpp), not these; typed text is parsed by
// staple::parseValue.
namespace eq1::valueText
{

// How many decimals a Frequency shows: below 100 Hz, from 100 Hz to 1 kHz, from 1 kHz to 10 kHz and from
// 10 kHz. The default is the Staple design's: whole Hz, then kHz to 2 decimals, 1 from 10 kHz.
struct FrequencyDecimals
{
    int belowHundredHz = 0, hz = 0, kHz = 2, fromTenKHz = 1;
};

// "85 Hz", "999 Hz", "1.00 kHz", "12.5 kHz". The unit and the decimals follow the value as shown: 999.7 Hz
// in whole Hz rounds to 1000, so it reads "1.00 kHz".
juce::String frequency (double hz, FrequencyDecimals decimals = {});

// A Frequency grid label: "20", "500", "1k", "20k".
juce::String frequencyLabel (double hz);

// How a signed dB value is written.
struct DbFormat
{
    int decimals = 1;
    bool unit = true;        // " dB" after it
    bool plusAtZero = false; // "+0.0 dB" rather than "0.0 dB"
};

// Signed, with a true minus sign (U+2212) and a plus where above zero: "+3.0 dB", "0.0 dB", "−3.0 dB".
// A value that rounds to zero is zero ("0.0 dB", never "−0.0 dB"); minus infinity is "−inf dB".
juce::String signedDb (double db, DbFormat format = {});

// A Slope: "12 dB/oct", or "37.5 dB/oct" where it is not within 0.05 of a whole number.
juce::String slope (double dbPerOctave);

// Whole percent, with a true minus below zero: "100%", "−40%".
juce::String percent (double value);

} // namespace eq1::valueText
