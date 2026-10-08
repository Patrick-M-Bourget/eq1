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

// Cuts, Notch, Band Pass and All Pass have no Gain: a Band with one of them keeps its stored Gain
// and ignores it, until its Shape changes back.
inline bool hasGain (Shape shape)
{
    return shape == Shape::Bell || shape == Shape::LowShelf || shape == Shape::HighShelf || shape == Shape::TiltShelf
           || shape == Shape::FlatTilt;
}

inline bool isCut (Shape shape) { return shape == Shape::LowCut || shape == Shape::HighCut; }

// Bell ignores Slope until Bell Slope (#19); Flat Tilt has none.
inline bool hasSlope (Shape shape) { return shape != Shape::Bell && shape != Shape::FlatTilt; }

// Which part of the stereo signal a Band processes. On a mono track the signal is all Mid, and Left
// and Right are the same signal, so a Side Band has no effect and the others process it.
enum class StereoPlacement
{
    Stereo,
    Left,
    Right,
    Mid,
    Side,
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
    StereoPlacement placement = StereoPlacement::Stereo;

    bool operator== (const BandSettings&) const = default;
};

inline constexpr int numBandSlots = 24;

// The full settings snapshot the Engine processes with.
struct Settings
{
    std::array<BandSettings, numBandSlots> bands {};

    // The Band Slot (1 to 24) being Soloed, or 0. Solo lasts while the editor holds it: it is not a
    // host parameter and is never saved. The output is then only the region of the input that Band
    // works on, on the part of the signal it processes.
    int soloSlot = 0;

    bool operator== (const Settings&) const = default;
};

} // namespace eq1
