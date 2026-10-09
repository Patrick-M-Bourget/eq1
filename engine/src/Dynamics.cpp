#include "Dynamics.h"

#include "Solo.h"

#include <algorithm>
#include <cmath>

namespace eq1
{

namespace
{
// The detector's level follows the detection signal's power over about this long.
constexpr double levelTimeConstantSeconds = 0.005;
// The gain computer's soft knee: movement starts this far below Threshold.
constexpr double kneeDb = 3.0;
// Auto Threshold sits this far above the average level of the region, which it follows over about
// this long. Levels below the gate (silence) don't pull it down.
constexpr double autoThresholdMarginDb = 4.0;
constexpr double autoThresholdTimeConstantSeconds = 2.0;
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
    averageCoefficient = coefficientFor (autoThresholdTimeConstantSeconds, sampleRate);
    dynamicRangeGlide.configure (glideTimeConstantSeconds * sampleRate, 1.0e-4);
    active.configure (glideTimeConstantSeconds * sampleRate, 1.0e-6);
    dynamicRangeGlide.reset (0.0);
    active.reset (0.0);
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
        samplesHeard = 0.0;
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
    movement = sustain = 0.0;
}

void Dynamics::setAuditioned (bool newAuditioned)
{
    if (newAuditioned && ! running())
        startAfresh();
    auditioned = newAuditioned;
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

double Dynamics::process (const float* const* input, int numChannels, const float* const* sidechain, int sidechainChannels, int numSamples)
{
    if (! running())
        return 0.0;

    const int detectionCount = detectionChannelCount = takeDetectionSignal (input, numChannels, sidechain, sidechainChannels, numSamples);
    if (detectionCount > 0)
    {
        rangeFilter.process (detectionChannels.data(), detectionCount, numSamples);
        highLimit.process (detectionChannels.data(), detectionCount, numSamples);
    }
    else
        power = {}; // nothing heard: a Sidechain connected again starts afresh

    // The level of each sample: the louder detection channel's power, where a full-scale sine reads 0 dB.
    std::array<double, Band::maxSubBlock> levels;
    double loudest = -1000.0;
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        double peakPower = 0.0;
        for (size_t c = 0; c < static_cast<size_t> (detectionCount); ++c)
        {
            const double x = detection[c][i];
            power[c] += powerCoefficient * (x * x - power[c]);
            peakPower = std::max (peakPower, power[c]);
        }
        levels[i] = 10.0 * std::log10 (2.0 * peakPower + 1.0e-30);
        // The mean of every level heard, until the time constant's worth has been: then the mean
        // over about the last time constant.
        if (levels[i] > autoThresholdGateDb)
        {
            samplesHeard = std::min (samplesHeard + 1.0, 1.0 / averageCoefficient);
            averageLevel += (levels[i] - averageLevel) / samplesHeard;
        }
        loudest = std::max (loudest, levels[i]);
    }

    // Until Auto Threshold has heard the region, nothing moves.
    const bool listening = ! thresholdAuto || samplesHeard > 0.0;
    const double thresholdDb = thresholdAuto ? averageLevel + autoThresholdMarginDb : threshold;
    const double span = 2.0 * std::abs (dynamicRange) + 2.0 * kneeDb;
    const double attackCoefficient = coefficientFor (autoAttackSeconds (loudest - thresholdDb) * attackScale, sampleRate);
    const double releaseCoefficient = coefficientFor (autoReleaseSeconds() * releaseScale, sampleRate);
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        // A soft knee from kneeDb below Threshold, then about 2:1 until the full Dynamic Range.
        const double x = listening ? std::clamp ((levels[i] - thresholdDb + kneeDb) / span, 0.0, 1.0) : 0.0;
        const double target = x * x * (3.0 - 2.0 * x);
        movement += (target > movement ? attackCoefficient : releaseCoefficient) * (target - movement);
        sustain += sustainCoefficient * (target - sustain);
    }

    dynamicRangeGlide.skip (numSamples);
    active.skip (numSamples);
    return movement * dynamicRangeGlide.value() * active.value();
}

} // namespace eq1
