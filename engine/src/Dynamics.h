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

    // While auditioned, the detector runs even if the Band isn't a Dynamic Band, so its detection
    // signal can be heard.
    void setAuditioned (bool auditioned);

    // While metered, the detector runs even if the Band isn't a Dynamic Band, so its Detection Level
    // can be read; only to measure, so the movement, Auto Threshold and glides wait as they do
    // unmetered, and the Band sounds the same metered or not.
    void setMetered (bool metered);

    // Listens to the piece of a run of the Engine's grid (Band::process) from position on, numSamples
    // long, of the main input, or of the Sidechain (sidechainChannels 0 when none is connected).
    void hear (const float* const* input, int numChannels, const float* const* sidechain, int sidechainChannels, int position,
               int numSamples);

    // At the end of a run: moves the gain computer over the levels the run heard, with its Auto
    // Attack and Auto Threshold taken over the whole run. Returns the offset in dB to add to the
    // Band's Gain over the next run.
    double finishRun();

    // The detection signal of the last hear() on output channel ch at sample i: the one detection
    // channel on every output channel, or each channel's own when there are two; silence when the
    // detector heard nothing.
    // A mono main output (outputChannels 1) hears the mean of two detection channels.
    float auditionSample (int outputChannels, int ch, int i) const;

    // The loudest level the detector compared with Threshold in the last hear(), in dB where a
    // full-scale sine reads 0; nothingHeardDb when it didn't run or had nothing to listen to.
    double detectionLevelDb() const { return loudestLevel; }
    static constexpr double nothingHeardDb = -1000.0;

private:
    bool running() const { return active.value() > 0.0 || active.isMoving() || auditioned; }
    // Clears the detector, Auto Threshold included, so it starts listening afresh.
    void startAfresh();

    // The detection signal: one channel, or two for a Stereo Band on a stereo source. 0 when the Band
    // has nothing to listen to: a Side Band on a mono main input, or External with no Sidechain.
    int takeDetectionSignal (const float* const* input, int numChannels, const float* const* sidechain, int sidechainChannels,
                             int numSamples);
    double autoAttackSeconds (double overshootDb) const;
    double autoReleaseSeconds() const;

    double sampleRate = 44100.0;
    StereoPlacement placement = StereoPlacement::Stereo;
    DetectionSource source = DetectionSource::Internal;
    double dynamicRange = 0.0;
    double threshold = -30.0;
    bool thresholdAuto = true;
    double attackScale = 1.0, releaseScale = 1.0; // times the Auto timing

    // The Detection Range's filters: rangeFilter is the Band's region, or the Free range's low limit;
    // highLimit is the Free range's high limit, not in use for a Band Detection Range.
    Band rangeFilter, highLimit;
    BandSettings rangeFilterSettings, highLimitSettings;
    bool auditioned = false;
    bool metered = false;
    double loudestLevel = nothingHeardDb;
    int detectionChannelCount = 0; // in the last hear()
    std::vector<std::array<float, Band::maxSubBlock>> detection;
    std::vector<float*> detectionChannels;

    // The levels the run in progress has heard while running, for the gain computer at its end.
    std::array<double, Band::maxSubBlock> runLevels {};
    int runLevelCount = 0;

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
