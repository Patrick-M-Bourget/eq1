#include "Band.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace eq1
{

namespace
{
// Glides and crossfades settle in about 7 time constants, so about 50 ms.
constexpr double glideTimeConstantSeconds = 0.007;
} // namespace

void Band::Chain::clear()
{
    for (auto& channel : states)
        channel = {};
}

void Band::Chain::run (size_t channel, float* samples, const Piece& piece)
{
    auto& channelStates = states[channel];
    for (size_t s = 0; s < static_cast<size_t> (cascade.count); ++s)
    {
        auto& state = channelStates[s];
        const auto& to = cascade.sections[s];
        if (piece.from == nullptr)
        {
            for (int i = 0; i < piece.numSamples; ++i)
                samples[i] = state.process (to, samples[i]);
            continue;
        }
        // Designed once per run, interpolated sample by sample by position in the run.
        const auto& start = piece.from->sections[s];
        for (int i = 0; i < piece.numSamples; ++i)
            samples[i] = state.process (interpolate (start, to, (piece.position + i + 1.0) / maxSubBlock), samples[i]);
    }
}

void Band::Chain::runPlaced (float* const* channels, int numChannels, const Piece& piece)
{
    switch (setup.placement)
    {
        case StereoPlacement::Stereo:
            for (int ch = 0; ch < numChannels; ++ch)
                run (static_cast<size_t> (ch), channels[ch], piece);
            return;
        case StereoPlacement::Left: run (0, channels[0], piece); return;
        case StereoPlacement::Right:
        {
            // On mono, Left and Right are the same signal.
            const int right = numChannels > 1 ? 1 : 0;
            run (static_cast<size_t> (right), channels[right], piece);
            return;
        }
        // On mono the signal is all Mid: a Mid Band processes it and a Side Band has nothing to process.
        case StereoPlacement::Mid:
            if (numChannels > 1)
                runMidSide (channels, piece, true);
            else
                run (0, channels[0], piece);
            return;
        case StereoPlacement::Side:
            if (numChannels > 1)
                runMidSide (channels, piece, false);
            return;
    }
}

void Band::Chain::runMidSide (float* const* channels, const Piece& piece, bool mid)
{
    std::array<float, maxSubBlock> midSamples, sideSamples;
    float* left = channels[0];
    float* right = channels[1];
    const auto numSamples = static_cast<size_t> (piece.numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        midSamples[i] = 0.5f * (left[i] + right[i]);
        sideSamples[i] = 0.5f * (left[i] - right[i]);
    }
    run (0, mid ? midSamples.data() : sideSamples.data(), piece);
    for (size_t i = 0; i < numSamples; ++i)
    {
        left[i] = midSamples[i] + sideSamples[i];
        right[i] = midSamples[i] - sideSamples[i];
    }
}

void Band::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    const double timeConstant = glideTimeConstantSeconds * sampleRate;
    // Close enough to stop gliding: 0.001% in Frequency and Q, 0.0001 dB in Gain, -120 dB in crossfades.
    logFrequency.configure (timeConstant, 1.0e-5);
    gain.configure (timeConstant, 1.0e-4);
    logQ.configure (timeConstant, 1.0e-5);
    mix.configure (timeConstant, 1.0e-6);
    shapeCrossfade.configure (timeConstant, 1.0e-6);
    for (auto* chain : { &current, &previous })
        chain->states.assign (static_cast<size_t> (numChannels), {});
    wet.assign (static_cast<size_t> (numChannels), {});
    previousWet.assign (static_cast<size_t> (numChannels), {});
    wetChannels.clear();
    previousWetChannels.clear();
    for (size_t ch = 0; ch < wet.size(); ++ch)
    {
        wetChannels.push_back (wet[ch].data());
        previousWetChannels.push_back (previousWet[ch].data());
    }
    mix.reset (0.0);
    shapeCrossfade.reset (1.0);
}

void Band::setSettings (const BandSettings& settings, bool snap)
{
    const double audible = settings.inUse && ! settings.bypass ? 1.0 : 0.0;
    const Setup setup { structureOf (settings), settings.placement };

    // A silent Band has nothing to glide or crossfade from: it takes its new settings at once and fades in.
    if (snap || isSilent())
    {
        logFrequency.reset (std::log (settings.frequency));
        gain.reset (settings.gain);
        logQ.reset (std::log (settings.q));
        current.setup = setup;
        current.clear();
        shapeCrossfade.reset (1.0);
        pending.reset();
        designNow();
    }
    else
    {
        logFrequency.setTarget (std::log (settings.frequency));
        gain.setTarget (settings.gain);
        logQ.setTarget (std::log (settings.q));
        // The crossfade starts with the next run.
        pending.reset();
        if (setup != current.setup)
            pending = setup;
    }

    if (snap)
        mix.reset (audible);
    else
        mix.setTarget (audible);
}

// The old filter keeps playing while the new one fades in. (Swapping moves the per-channel state
// without allocating.)
void Band::crossfadeTo (const Setup& setup)
{
    std::swap (current, previous);
    current.setup = setup;
    current.clear();
    design();
    shapeCrossfade.reset (0.0);
    shapeCrossfade.setTarget (1.0);
}

// Between the run's start and end as the coefficients are: near enough, for the display.
double Band::liveGainDb() const
{
    return runStartGainDb + (runEndGainDb - runStartGainDb) * runPosition / maxSubBlock;
}

double Band::targetGainDb() const
{
    return std::clamp (gain.value() + dynamicOffset, -liveGainLimitDb, liveGainLimitDb);
}

void Band::design()
{
    designedOffset = dynamicOffset;
    runEndGainDb = targetGainDb();
    current.cascade = designShape ({ current.setup.structure, std::exp (logFrequency.value()), runEndGainDb, std::exp (logQ.value()) },
                                   sampleRate);
}

void Band::designNow()
{
    design();
    runGliding = false;
    runStartGainDb = runEndGainDb;
}

void Band::startRun()
{
    if (pending && ! shapeCrossfade.isMoving())
    {
        crossfadeTo (*pending);
        pending.reset();
    }

    // A Dynamic Band's moving Live Gain glides its filter as a Gain change does.
    runGliding = logFrequency.isMoving() || gain.isMoving() || logQ.isMoving() || dynamicOffset != designedOffset;
    runStartGainDb = runEndGainDb;
    if (runGliding)
    {
        runFrom = current.cascade;
        logFrequency.skip (maxSubBlock);
        gain.skip (maxSubBlock);
        logQ.skip (maxSubBlock);
        design();
    }
}

void Band::process (float* const* channels, int numChannels, int position, int numSamples)
{
    const bool joinedLate = position != 0 && position != runPosition;
    runPosition = position + numSamples;
    if (isSilent())
        return;

    if (position == 0)
        startRun();
    else if (joinedLate)
    {
        runGliding = false;
        runStartGainDb = runEndGainDb;
    }

    const bool fading = mix.isMoving();
    const bool crossfading = shapeCrossfade.isMoving();
    const Chain::Piece piece { position, numSamples, runGliding ? &runFrom : nullptr };

    if (! fading && ! crossfading && mix.value() == 1.0)
    {
        current.runPlaced (channels, numChannels, piece);
        return;
    }

    std::array<double, maxSubBlock> mixes, crossfades;
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        mixes[i] = fading ? mix.next() : mix.value();
        crossfades[i] = crossfading ? shapeCrossfade.next() : shapeCrossfade.value();
    }

    // A channel the chains leave alone comes out of the mix exactly as it went in.
    const auto copyInput = [&] (std::vector<float*>& to) {
        for (int ch = 0; ch < numChannels; ++ch)
            std::copy (channels[ch], channels[ch] + numSamples, to[static_cast<size_t> (ch)]);
    };
    copyInput (wetChannels);
    current.runPlaced (wetChannels.data(), numChannels, piece);
    if (crossfading)
    {
        copyInput (previousWetChannels);
        previous.runPlaced (previousWetChannels.data(), numChannels, { position, numSamples });
    }
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* samples = channels[ch];
        auto& wetSamples = wet[static_cast<size_t> (ch)];
        const auto& previousSamples = previousWet[static_cast<size_t> (ch)];
        for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
        {
            if (crossfading)
                wetSamples[i] = static_cast<float> (previousSamples[i] + crossfades[i] * (wetSamples[i] - previousSamples[i]));
            samples[i] = static_cast<float> (samples[i] + mixes[i] * (wetSamples[i] - samples[i]));
        }
    }

    // Faded out: start clean when the Band next fades in.
    if (isSilent())
    {
        current.clear();
        previous.clear();
    }
}

} // namespace eq1
