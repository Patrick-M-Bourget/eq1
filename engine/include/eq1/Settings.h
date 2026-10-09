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

// The Shapes that can be dynamic: those with a Gain to move. A Band with another Shape keeps its
// dynamics settings and ignores them, until its Shape changes back.
inline bool hasDynamics (Shape shape) { return hasGain (shape); }

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

// The signal a Dynamic Band listens to: the main input, or the Sidechain.
enum class DetectionSource
{
    Internal,
    External,
};

// The part of the spectrum a Dynamic Band's detector listens to: the Band's own region, or between
// a user-set low and high limit.
enum class DetectionRange
{
    Band,
    Free,
};

// What Output Pan balances: Left against Right, or Mid against Side (left of centre keeps the Mid).
enum class PanMode
{
    LeftRight,
    MidSide,
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

    // Dynamics. A Band is a Dynamic Band when its Shape has dynamics and dynamicRange is not 0: its
    // Live Gain then moves from Gain towards Gain + dynamicRange as its detection signal rises above
    // Threshold, never beyond +/-30 dB. Detection is on its Detection Source, in its Detection Range
    // (docs/dsp/filter-design.md, "Dynamics"), on the part of the signal the Band processes.
    DetectionSource detectionSource = DetectionSource::Internal;
    DetectionRange detectionRange = DetectionRange::Band;
    double detectionLow = 20.0;     // Hz, the Free Detection Range's limits
    double detectionHigh = 20000.0; // Hz
    double dynamicRange = 0.0;  // dB, -30 to 30
    double threshold = -30.0;   // dB, where a full-scale sine reads 0; ignored when thresholdAuto
    bool thresholdAuto = true;  // Threshold keeps adapting to the level of the Band's region
    double attack = 50.0;       // %, 0 to 100: 50 is Auto, lower is faster, higher slower
    double release = 50.0;      // %, as attack
    bool dynamicsBypass = false; // holds Live Gain at Gain, keeping every setting

    bool operator== (const BandSettings&) const = default;
};

// Live Gain never goes beyond +/- this many dB, whatever Gain and Dynamic Range are.
inline constexpr double liveGainLimitDb = 30.0;

// True when the Band's Live Gain moves with its detection signal.
inline bool isDynamic (const BandSettings& band) { return hasDynamics (band.shape) && band.dynamicRange != 0.0; }

inline constexpr int numBandSlots = 24;

// The Band as it plays under Gain Scale: its Gain and Dynamic Range scaled, on Shapes that have a Gain.
inline BandSettings scaledByGainScale (BandSettings band, double gainScale)
{
    if (hasGain (band.shape))
    {
        band.gain *= gainScale;
        band.dynamicRange *= gainScale;
    }
    return band;
}

// The full settings snapshot the Engine processes with.
struct Settings
{
    std::array<BandSettings, numBandSlots> bands {};

    // The Band Slot (1 to 24) being Soloed, or 0. Solo lasts while the editor holds it: it is not a
    // host parameter and is never saved. The output is then only the region of the input that Band
    // works on, on the part of the signal it processes.
    int soloSlot = 0;

    // The Band Slot (1 to 24) whose Detection Audition is held, or 0. Like Solo, it lasts while the
    // editor holds it and is never saved. The output is then what that Band's detector hears: its
    // detection signal after the Detection Range, on every channel, or each channel's own for a
    // Stereo Band on a stereo source. Only a Band in use whose Shape has dynamics can be auditioned.
    // It takes precedence over Solo.
    int auditionSlot = 0;

    // Gain Scale, 0 to 2 (0% to 200%): scales every Band's Gain and Dynamic Range in dB, on the Shapes
    // that have a Gain. Live Gain stays within +/-30 dB.
    double gainScale = 1.0;

    // Auto Gain: compensates the output level by an estimate from the settings (eq1/Response.h,
    // autoGainDb), not a measurement, so it doesn't follow dynamic movement.
    bool autoGain = false;

    // The output, after every Band and Auto Gain. Output Gain is in dB, -infinity (silence) to +36. Output Pan, -1
    // to 1, balances the two sides of its Pan Mode: the centre leaves both alone, and moving towards
    // one side turns the other down, to silence at the end. Pan and Pan Mode have no effect on mono.
    double outputGainDb = 0.0;
    double outputPan = 0.0;
    PanMode panMode = PanMode::LeftRight;
    bool phaseInvert = false;

    // Global Bypass: eq1's own switch, separate from the host's bypass. It crossfades to the input,
    // unprocessed by anything above, and back.
    bool globalBypass = false;

    bool operator== (const Settings&) const = default;
};

} // namespace eq1
