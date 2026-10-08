#pragma once

#include <array>

namespace eq1
{

enum class Shape
{
    Bell,
};

// One Band slot. A slot not in use, or a Bypassed Band, has no effect but keeps its settings.
struct BandSettings
{
    bool inUse = false;
    bool bypass = false;
    Shape shape = Shape::Bell;
    double frequency = 1000.0; // Hz
    double gain = 0.0;         // dB
    double q = 1.0;

    bool operator== (const BandSettings&) const = default;
};

inline constexpr int numBandSlots = 24;

// The full settings snapshot the Engine processes with.
struct Settings
{
    std::array<BandSettings, numBandSlots> bands {};

    bool operator== (const Settings&) const = default;
};

} // namespace eq1
