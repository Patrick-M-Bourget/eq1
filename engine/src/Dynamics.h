#pragma once

#include "Band.h"
#include "Smoother.h"
#include "eq1/Settings.h"

#include <array>
#include <vector>

namespace eq1
{

// The detector and gain computer of one Band Slot (docs/dsp/filter-design.md, "Dynamics"). It
// listens to the main input in the Band's detection region, on the part of the signal the Band
// processes, and says how far the Band's Live Gain moves away from its Gain.
class Dynamics
{
public:
    void prepare (double sampleRate);

    // With snap, the new settings take effect at once and detection starts afresh.
    void setSettings (const BandSettings& settings, bool snap);

    // Listens to a run of at most Band::maxSubBlock samples of the main input. Returns the offset in
    // dB to add to the Band's Gain at the end of the run.
    double process (const float* const* input, int numChannels, int numSamples);

private:
    // The detection signal: one channel, or two for a Stereo Band on a stereo track. 0 when the Band
    // has nothing to listen to: a Side Band on mono.
    int takeDetectionSignal (const float* const* input, int numChannels, int numSamples);
    double autoAttackSeconds (double overshootDb) const;
    double autoReleaseSeconds() const;

    double sampleRate = 44100.0;
    StereoPlacement placement = StereoPlacement::Stereo;
    double dynamicRange = 0.0;
    double threshold = -30.0;
    bool thresholdAuto = true;
    double attackScale = 1.0, releaseScale = 1.0; // times the Auto timing

    Band region; // the detection region's filter
    std::vector<std::array<float, Band::maxSubBlock>> detection;
    std::vector<float*> detectionChannels;

    std::array<double, 2> power {}; // per detection channel, smoothed
    double powerCoefficient = 1.0;
    double movement = 0.0; // 0 = at Gain, 1 = the full Dynamic Range
    double sustain = 0.0;  // how long the detection has kept the Band moving, 0 to 1
    double sustainCoefficient = 1.0;
    double averageLevel = 0.0; // dB, for Auto Threshold
    double samplesHeard = 0.0; // above the gate, up to the average's time constant
    double averageCoefficient = 1.0;

    Smoother dynamicRangeGlide; // dB
    Smoother active;     // 1 while the Band's Shape has dynamics and they aren't Bypassed
};

} // namespace eq1
