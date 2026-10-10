#include "staple/controls/ParseValue.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("parseValue reads a typed value in its parameter's units, with or without a unit or a k")
{
    struct Row
    {
        const char* text;
        const char* unit;
        double expected;
    };
    const Row rows[] = {
        { "1.2k", "Hz", 1200.0 },   { "1200", "Hz", 1200.0 },  { "1.2 kHz", "Hz", 1200.0 }, { "1.2KHZ", "Hz", 1200.0 },
        { "450 Hz", "Hz", 450.0 },  { "+3", "dB", 3.0 },       { "-2.5 dB", "dB", -2.5 },   { "0.7", "", 0.7 },
        { ".5", "", 0.5 },          { "15 ms", "ms", 15.0 },   { "1.2 s", "ms", 1200.0 },   { "1.2 s", "s", 1.2 },
        { "15 ms", "s", 0.015 },    { " 2,5 dB ", "dB", 2.5 }, { "40 %", "%", 40.0 },
    };
    for (const auto& row : rows)
    {
        CAPTURE (row.text, row.unit);
        const auto value = staple::parseValue (row.text, row.unit);
        REQUIRE (value.has_value());
        CHECK_THAT (*value, WithinAbs (row.expected, 1.0e-9));
    }
}

TEST_CASE ("parseValue reads nothing from text that isn't a number with a unit it knows")
{
    for (const char* text : { "", "abc", "Auto", "1.2 parsecs", "3 dB dB", "--3", "k" })
    {
        CAPTURE (text);
        CHECK_FALSE (staple::parseValue (text, "dB").has_value());
    }
}
