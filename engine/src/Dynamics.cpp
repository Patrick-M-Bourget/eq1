#include "Dynamics.h"

#include "Placement.h"
#include "Smoothstep.h"
#include "Solo.h"
#include "Units.h"

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
// Coming to rest after the music stops: the movement and sustain release exponentially, so they
// snap to exactly 0 below this much (a billionth of the Dynamic Range: 3e-8 dB at its largest). The
// Band's offset is then exactly 0 run after run, and it neither glides nor redesigns its filter.
constexpr double movementRest = 1.0e-9;

// A Free Detection Range's limits roll off at this Slope, steep enough to pick out a kick alone.
constexpr double freeRangeSlope = 24.0;

// A sample's level from the louder detection channel's smoothed power, where a full-scale sine reads
// 0 dB; the floor keeps silence finite.
double levelOf (double peakPower) { return powerToDecibels (2.0 * peakPower + 1.0e-30); }
// What a detector with nothing to listen to hears: the level of no power at all.
const double nothingToHearDb = levelOf (0.0);

double coefficientFor (double seconds, double sampleRate) { return onePoleCoefficient (seconds * sampleRate); }

double timingScale (double percent) { return std::pow (timingSpread, (std::clamp (percent, 0.0, 100.0) - 50.0) / 50.0); }
// A Free Detection Range's limit: a Butterworth Cut at frequency.
BandSettings freeLimit (Shape cut, double frequency)
{
    return { .inUse = true, .shape = cut, .frequency = frequency, .q = butterworthQ, .slope = freeRangeSlope };
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
    // The part the Band processes, of the first two channels.
    return routePlacement (
        placement, std::min (numChannels, 2), [&] (int part, int ch) { copy (static_cast<size_t> (part), input[ch]); },
        [&] (bool mid) {
            for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
                detection[0][i] = mid ? encodeMid (input[0][i], input[1][i]) : encodeSide (input[0][i], input[1][i]);
        });
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
    // A run is at most maxSubBlock samples, and finishRun() empties the levels at its end.
    assert (runLevelCount + numSamples <= Band::maxSubBlock);
    if (detectionCount == 0)
    {
        power = {}; // nothing heard: a Sidechain connected again starts afresh
        // The gain computer hears silence, so the Band returns to its Gain over the run.
        if (moving)
        {
            const auto count = static_cast<size_t> (std::min (numSamples, Band::maxSubBlock - runLevelCount));
            std::fill_n (runLevels.begin() + runLevelCount, count, nothingToHearDb);
            runLevelCount += static_cast<int> (count);
        }
        return;
    }
    rangeFilter.process (detectionChannels.data(), detectionCount, run, position, numSamples);
    highLimit.process (detectionChannels.data(), detectionCount, run, position, numSamples);

    // The level of each sample: the louder detection channel's power. Metered only, the detector
    // keeps the piece's loudest power and converts it once at the end: the level is monotonic in
    // the power, so it is the loudest level, and whatever comes next starts afresh, as if never
    // metered.
    double loudest = nothingHeardDb, loudestPower = 0.0;
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        double peakPower = 0.0;
        for (size_t c = 0; c < static_cast<size_t> (detectionCount); ++c)
        {
            const double x = detection[c][i];
            power[c] += powerCoefficient * (x * x - power[c]);
            peakPower = std::max (peakPower, power[c]);
        }
        if (! moving)
        {
            loudestPower = std::max (loudestPower, peakPower);
            continue;
        }
        const double level = levelOf (peakPower);
        loudest = std::max (loudest, level);
        if (runLevelCount < Band::maxSubBlock)
            runLevels[static_cast<size_t> (runLevelCount++)] = level;
        // The mean and variance of every level heard, until a rise time's worth has been: then
        // rising over about the rise time and falling over about the fall time.
        if (level > autoThresholdGateDb)
        {
            samplesHeard = std::min (samplesHeard + 1.0, 1.0 / riseCoefficient);
            const double meanWeight = 1.0 / samplesHeard;
            const auto weight = [&] (bool rising) { return std::max (rising ? riseCoefficient : fallCoefficient, meanWeight); };
            averageLevel += weight (level > averageLevel) * (level - averageLevel);
            const double deviation = (level - averageLevel) * (level - averageLevel);
            levelVariance += weight (deviation > levelVariance) * (deviation - levelVariance);
        }
    }
    loudestLevel = moving ? loudest : levelOf (loudestPower);
}

double Dynamics::finishRun()
{
    const auto levels = std::span (runLevels).first (static_cast<size_t> (runLevelCount));
    runLevelCount = 0;
    // A NaN or infinite sample the detector heard would stay in its level and its statistics for
    // good, and so in the Band's Live Gain. The detector starts afresh instead, Auto Threshold
    // included, as it does when the Band becomes active, and the Band rests at its Gain over the
    // next run. (A sum of finite numbers is finite, and one non-finite term makes it not.)
    if (! std::isfinite (power[0] + power[1] + movement + sustain + averageLevel + levelVariance))
    {
        startAfresh();
        averageLevel = levelVariance = 0.0;
        return 0.0;
    }
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
        const double target = listening ? smoothstep ((level - thresholdDb + kneeDb) / span) : 0.0;
        movement += (target > movement ? attackCoefficient : releaseCoefficient) * (target - movement);
        sustain += sustainCoefficient * (target - sustain);
    }
    if (movement < movementRest)
        movement = 0.0;
    if (sustain < movementRest)
        sustain = 0.0;

    dynamicRangeGlide.skip (Band::maxSubBlock);
    active.skip (Band::maxSubBlock);
    return movement * dynamicRangeGlide.value() * active.value();
}

} // namespace eq1
