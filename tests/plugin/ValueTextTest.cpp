#include "ValueText.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace valueText = eq1::valueText;

TEST_CASE ("A Frequency reads in whole Hz below 1 kHz, then kHz to 2 decimals, 1 from 10 kHz")
{
    CHECK (valueText::frequency (20.0) == "20 Hz");
    CHECK (valueText::frequency (85.4) == "85 Hz");
    CHECK (valueText::frequency (999.4) == "999 Hz");
    // Rounding to 1000 Hz goes over to kHz.
    CHECK (valueText::frequency (999.9) == "1.00 kHz");
    CHECK (valueText::frequency (1000.0) == "1.00 kHz");
    CHECK (valueText::frequency (4500.0) == "4.50 kHz");
    CHECK (valueText::frequency (9994.0) == "9.99 kHz");
    // Rounding to 10.00 kHz goes over to 1 decimal.
    CHECK (valueText::frequency (9996.0) == "10.0 kHz");
    CHECK (valueText::frequency (12500.0) == "12.5 kHz");
    CHECK (valueText::frequency (20000.0) == "20.0 kHz");
}

TEST_CASE ("A Frequency's decimals can differ in each of its four spans")
{
    const valueText::FrequencyDecimals card { .belowHundredHz = 1, .hz = 0, .kHz = 2, .fromTenKHz = 2 };
    CHECK (valueText::frequency (85.0, card) == "85.0 Hz");
    CHECK (valueText::frequency (99.94, card) == "99.9 Hz");
    // 99.96 shows as 100.0 at 1 decimal, so it is past 100 Hz: whole Hz.
    CHECK (valueText::frequency (99.96, card) == "100 Hz");
    CHECK (valueText::frequency (850.0, card) == "850 Hz");
    CHECK (valueText::frequency (12500.0, card) == "12.50 kHz");

    const valueText::FrequencyDecimals ghost { .belowHundredHz = 1, .hz = 1 };
    CHECK (valueText::frequency (850.0, ghost) == "850.0 Hz");
    CHECK (valueText::frequency (999.94, ghost) == "999.9 Hz");
    CHECK (valueText::frequency (999.96, ghost) == "1.00 kHz");
    CHECK (valueText::frequency (12500.0, ghost) == "12.5 kHz");
}

TEST_CASE ("A Frequency grid label is whole Hz below 1 kHz, then whole kHz with a k")
{
    CHECK (valueText::frequencyLabel (20.0) == "20");
    CHECK (valueText::frequencyLabel (500.0) == "500");
    CHECK (valueText::frequencyLabel (1000.0) == "1k");
    CHECK (valueText::frequencyLabel (20000.0) == "20k");
}

TEST_CASE ("Signed dB has a plus above zero, a true minus below, and no sign at zero")
{
    const auto minus = juce::String::charToString (0x2212);
    CHECK (valueText::signedDb (3.0) == "+3.0 dB");
    CHECK (valueText::signedDb (-3.0) == minus + "3.0 dB");
    CHECK (valueText::signedDb (0.0) == "0.0 dB");
    CHECK (valueText::signedDb (-0.0) == "0.0 dB");
    // Rounding: to the decimals shown, and a value that rounds to zero is zero.
    CHECK (valueText::signedDb (2.25, { .decimals = 2 }) == "+2.25 dB");
    CHECK (valueText::signedDb (-1.84) == minus + "1.8 dB");
    CHECK (valueText::signedDb (0.04) == "0.0 dB");
    CHECK (valueText::signedDb (-0.004, { .decimals = 2 }) == "0.00 dB");
    CHECK (valueText::signedDb (-12.0, { .decimals = 2 }) == minus + "12.00 dB");
    // Whole, without the unit, as the grid's labels.
    CHECK (valueText::signedDb (6.0, { .decimals = 0, .unit = false }) == "+6");
    CHECK (valueText::signedDb (-6.0, { .decimals = 0, .unit = false }) == minus + "6");
    CHECK (valueText::signedDb (0.0, { .decimals = 0, .unit = false }) == "0");
    // A plus at zero, as the Output readout shows.
    CHECK (valueText::signedDb (0.0, { .plusAtZero = true }) == "+0.0 dB");
    CHECK (valueText::signedDb (-0.04, { .plusAtZero = true }) == "+0.0 dB");
    CHECK (valueText::signedDb (-std::numeric_limits<double>::infinity()) == minus + "inf dB");
}

TEST_CASE ("A Slope reads in whole dB/oct, or to 1 decimal off a whole number")
{
    CHECK (valueText::slope (12.0) == "12 dB/oct");
    CHECK (valueText::slope (36.97) == "37 dB/oct");
    CHECK (valueText::slope (37.5) == "37.5 dB/oct");
    CHECK (valueText::slope (4.5) == "4.5 dB/oct");
    CHECK (valueText::slope (0.5) == "0.5 dB/oct");
}

TEST_CASE ("A percent is whole")
{
    CHECK (valueText::percent (100.0) == "100%");
    CHECK (valueText::percent (49.6) == "50%");
    CHECK (valueText::percent (0.0) == "0%");
}
