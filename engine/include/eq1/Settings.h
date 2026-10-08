#pragma once

#include <array>

namespace eq1
{

// In Pro-Q 4's order, which the host parameter follows (ADR 0003).
enum class Shape
{
    Bell,
    LowShelf,
    LowCut,
    HighShelf,
    HighCut,
    Notch,
    BandPass,
    TiltShelf,
    FlatTilt,
    AllPass,
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
    // dB/oct, 0 to 96. Kept as set: the Engine raises it to the Shape's minimum and rounds it to a
    // whole order, so switching Shape and back restores it (ADR 0003).
    double slope = 12.0;
    bool brickwall = false; // Low Cut and High Cut only: overrides Slope

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
