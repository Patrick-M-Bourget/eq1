#include "Dynamics.h"

#include "Solo.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <span>

namespace eq1
{

namespace
{
// The detector's level follows the detection signal's power over about this long.
constexpr double levelTimeConstantSeconds = 0.005;
// The gain computer's soft knee: movement starts this far below Threshold.
constexpr double kneeDb = 3.0;
// The full Dynamic Range arrives this far above Threshold (plus the knee), whatever its size.
constexpr double fullRangeOvershootDb = 12.0;
// Auto Threshold is the region's mean level plus this many standard deviations of it, both in dB.
// They rise over about autoThresholdRiseSeconds, so a swell moves the Band before it is learned, and
// fall over about autoThresholdFallSeconds, so a quieter passage is followed promptly. Levels below
// the gate (silence) don't count, and nothing moves until the region has been heard for the hold.
constexpr double autoThresholdSpreads = 0.2;
constexpr double autoThresholdRiseSeconds = 4.5, autoThresholdFallSeconds = 1.25;
constexpr double autoThresholdHoldSeconds = 0.25;
constexpr double autoThresholdGateDb = -80.0;
// Auto Attack: this slow just above Threshold, faster the further above it the detection goes.
constexpr double autoAttackSlowestSeconds = 0.020;
// Auto Release: this fast after a short burst, up to this much slower after sustained movement,
// which it measures over about sustainTimeConstantSeconds.
constexpr double autoReleaseFastestSeconds = 0.040, autoReleaseSustainedSeconds = 0.500;
constexpr double sustainTimeConstantSeconds = 0.5;
// Attack and Release at 0% and 100% are the Auto timing times 1/10 and 10.
constexpr double timingSpread = 10.0;
// Dynamic Range and Dynamics Bypass changes glide like a Band's settings: about 50 ms.
constexpr double glideTimeConstantSeconds = 0.007;

// A Free Detection Range's limits roll off at this Slope, steep enough to pick out a kick alone.
constexpr double freeRangeSlope = 24.0;

double coefficientFor (double seconds, double sampleRate) { return 1.0 - std::exp (-1.0 / (seconds * sampleRate)); }

double timingScale (double percent) { return std::pow (timingSpread, (std::clamp (percent, 0.0, 100.0) - 50.0) / 50.0); }
// A Free Detection Range's limit: a Butterworth Cut at frequency.
BandSettings freeLimit (Shape cut, double frequency)
{
    return { .inUse = true, .shape = cut, .frequency = frequency, .q = std::sqrt (0.5), .slope = freeRangeSlope };
}
} // namespace

void Dynamics::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    rangeFilter.prepare (sampleRate, 2);
    highLimit.prepare (sampleRate, 2);
    detection.assign (2, {});
    detectionChannels.clear();
    for (auto& channel : detection)
        detectionChannels.push_back (channel.data());
    powerCoefficient = coefficientFor (levelTimeConstantSeconds, sampleRate);
    sustainCoefficient = coefficientFor (sustainTimeConstantSeconds, sampleRate);
    riseCoefficient = coefficientFor (autoThresholdRiseSeconds, sampleRate);
    fallCoefficient = coefficientFor (autoThresholdFallSeconds, sampleRate);
    holdSamples = autoThresholdHoldSeconds * sampleRate;
    dynamicRangeGlide.configure (glideTimeConstantSeconds * sampleRate, 1.0e-4);
    active.configure (glideTimeConstantSeconds * sampleRate, 1.0e-6);
    dynamicRangeGlide.reset (0.0);
    active.reset (0.0);
    runLevelCount = 0;
}

void Dynamics::setSettings (const BandSettings& settings, bool snap)
{
    placement = settings.placement;
    source = settings.detectionSource;
    dynamicRange = settings.dynamicRange;
    threshold = settings.threshold;
    thresholdAuto = settings.thresholdAuto;
    attackScale = timingScale (settings.attack);
    releaseScale = timingScale (settings.release);

    const bool wasRunning = running();
    const double activeTarget = settings.inUse && ! settings.bypass && isDynamic (settings) && ! settings.dynamicsBypass ? 1.0 : 0.0;
    if (settings.detectionRange == DetectionRange::Free)
    {
        rangeFilterSettings = freeLimit (Shape::LowCut, settings.detectionLow);
        highLimitSettings = freeLimit (Shape::HighCut, settings.detectionHigh);
    }
    else
    {
        rangeFilterSettings = soloRegionOf (settings);
        highLimitSettings = {}; // not in use: passes everything
    }
    if (snap || (! wasRunning && activeTarget > 0.0))
    {
        startAfresh();
    }
    else
    {
        rangeFilter.setSettings (rangeFilterSettings, false);
        highLimit.setSettings (highLimitSettings, false);
    }
    if (snap)
    {
        dynamicRangeGlide.reset (dynamicRange);
        active.reset (activeTarget);
    }
    else
    {
        dynamicRangeGlide.setTarget (dynamicRange);
        active.setTarget (activeTarget);
    }
}

void Dynamics::startAfresh()
{
    rangeFilter.setSettings (rangeFilterSettings, true);
    highLimit.setSettings (highLimitSettings, true);
    power = {};
    runLevelCount = 0;
    movement = sustain = 0.0;
    samplesHeard = 0.0; // Auto Threshold learns the material playing now
}

void Dynamics::setAuditioned (bool newAuditioned)
{
    if (newAuditioned && ! running())
        startAfresh();
    auditioned = newAuditioned;
}

void Dynamics::setMetered (bool newMetered)
{
    if (newMetered && ! metered && ! running())
        startAfresh();
    metered = newMetered;
}

float Dynamics::auditionSample (int outputChannels, int ch, int i) const
{
    const auto at = [&] (size_t c) { return detection[c][static_cast<size_t> (i)]; };
    if (detectionChannelCount == 0 || (detectionChannelCount == 2 && ch > 1))
        return 0.0f;
    if (detectionChannelCount == 1)
        return at (0);
    return outputChannels == 1 ? 0.5f * (at (0) + at (1)) : at (static_cast<size_t> (ch));
}

int Dynamics::takeDetectionSignal (const float* const* input, int numChannels, const float* const* sidechain, int sidechainChannels,
                                   int numSamples)
{
    const auto copy = [&] (size_t to, const float* from) { std::copy (from, from + numSamples, detection[to].data()); };
    if (source == DetectionSource::External)
    {
        if (sidechainChannels <= 0)
            return 0;
        // A mono Sidechain is the detection signal for every Stereo Placement, Side included, so
        // a mono kick on the Sidechain always works. A stereo one follows the main input's rules.
        if (sidechainChannels == 1)
        {
            copy (0, sidechain[0]);
            return 1;
        }
        input = sidechain;
        numChannels = sidechainChannels;
    }
    if (numChannels < 2)
    {
        // On mono the signal is all Mid, and Left and Right are the same signal.
        if (placement == StereoPlacement::Side)
            return 0;
        copy (0, input[0]);
        return 1;
    }
    switch (placement)
    {
        case StereoPlacement::Stereo:
            copy (0, input[0]);
            copy (1, input[1]);
            return 2;
        case StereoPlacement::Left: copy (0, input[0]); return 1;
        case StereoPlacement::Right: copy (0, input[1]); return 1;
        case StereoPlacement::Mid:
        case StereoPlacement::Side:
        {
            const float sign = placement == StereoPlacement::Mid ? 1.0f : -1.0f;
            for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
                detection[0][i] = 0.5f * (input[0][i] + sign * input[1][i]);
            return 1;
        }
    }
    return 0;
}

double Dynamics::autoAttackSeconds (double overshootDb) const
{
    return autoAttackSlowestSeconds * kneeDb / (kneeDb + std::max (overshootDb, 0.0));
}

double Dynamics::autoReleaseSeconds() const
{
    return autoReleaseFastestSeconds + (autoReleaseSustainedSeconds - autoReleaseFastestSeconds) * sustain;
}

void Dynamics::hear (const float* const* input, int numChannels, const float* const* sidechain, int sidechainChannels, std::uint64_t run,
                     int position, int numSamples)
{
    loudestLevel = nothingHeardDb;
    const bool moving = running();
    if (! moving && ! metered)
        return;

    const int detectionCount = detectionChannelCount = takeDetectionSignal (input, numChannels, sidechain, sidechainChannels, numSamples);
    if (detectionCount > 0)
    {
        rangeFilter.process (detectionChannels.data(), detectionCount, run, position, numSamples);
        highLimit.process (detectionChannels.data(), detectionCount, run, position, numSamples);
    }
    else
        power = {}; // nothing heard: a Sidechain connected again starts afresh

    // The level of each sample: the louder detection channel's power, where a full-scale sine reads 0 dB.
    double loudest = nothingHeardDb;
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        double peakPower = 0.0;
        for (size_t c = 0; c < static_cast<size_t> (detectionCount); ++c)
        {
            const double x = detection[c][i];
            power[c] += powerCoefficient * (x * x - power[c]);
            peakPower = std::max (peakPower, power[c]);
        }
        const double level = 10.0 * std::log10 (2.0 * peakPower + 1.0e-30);
        loudest = std::max (loudest, level);
        // Metered only: whatever comes next starts afresh, as if never metered.
        if (! moving)
            continue;
        // A run is at most maxSubBlock samples, and finishRun() empties the levels at its end.
        assert (runLevelCount < Band::maxSubBlock);
        if (runLevelCount < Band::maxSubBlock)
            runLevels[static_cast<size_t> (runLevelCount++)] = level;
        // The mean and variance of every level heard, until a rise time's worth has been: then
        // rising over about the rise time and falling over about the fall time.
        if (level > autoThresholdGateDb)
        {
            samplesHeard = std::min (samplesHeard + 1.0, 1.0 / riseCoefficient);
            const auto weight = [&] (bool rising) { return std::max (rising ? riseCoefficient : fallCoefficient, 1.0 / samplesHeard); };
            averageLevel += weight (level > averageLevel) * (level - averageLevel);
            const double deviation = (level - averageLevel) * (level - averageLevel);
            levelVariance += weight (deviation > levelVariance) * (deviation - levelVariance);
        }
    }
    if (detectionCount > 0)
        loudestLevel = loudest;
}

double Dynamics::finishRun()
{
    const auto levels = std::span (runLevels).first (static_cast<size_t> (runLevelCount));
    runLevelCount = 0;
    if (! running())
        return 0.0;

    // Until Auto Threshold has heard the region for the hold, nothing moves.
    const bool listening = ! thresholdAuto || samplesHeard >= holdSamples;
    const double thresholdDb = thresholdAuto ? averageLevel + autoThresholdSpreads * std::sqrt (levelVariance) : threshold;
    const double span = fullRangeOvershootDb + 2.0 * kneeDb;
    const double loudest = levels.empty() ? nothingHeardDb : *std::max_element (levels.begin(), levels.end());
    const double attackCoefficient = coefficientFor (autoAttackSeconds (loudest - thresholdDb) * attackScale, sampleRate);
    const double releaseCoefficient = coefficientFor (autoReleaseSeconds() * releaseScale, sampleRate);
    for (const double level : levels)
    {
        // A soft knee from kneeDb below Threshold, then smoothly to the full Dynamic Range at
        // fullRangeOvershootDb + kneeDb above it.
        const double x = listening ? std::clamp ((level - thresholdDb + kneeDb) / span, 0.0, 1.0) : 0.0;
        const double target = x * x * (3.0 - 2.0 * x);
        movement += (target > movement ? attackCoefficient : releaseCoefficient) * (target - movement);
        sustain += sustainCoefficient * (target - sustain);
    }

    dynamicRangeGlide.skip (Band::maxSubBlock);
    active.skip (Band::maxSubBlock);
    return movement * dynamicRangeGlide.value() * active.value();
}

} // namespace eq1
