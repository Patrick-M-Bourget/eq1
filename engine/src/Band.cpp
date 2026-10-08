#include "Band.h"

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

void Band::Chain::run (size_t channel, float* samples, int numSamples, const Cascade* from)
{
    auto& channelStates = states[channel];
    for (size_t s = 0; s < static_cast<size_t> (cascade.count); ++s)
    {
        auto& state = channelStates[s];
        const auto& to = cascade.sections[s];
        if (from == nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
                samples[i] = state.process (to, samples[i]);
            continue;
        }
        // Designed once per sub-block, interpolated sample by sample in between.
        const auto& start = from->sections[s];
        for (int i = 0; i < numSamples; ++i)
            samples[i] = state.process (interpolate (start, to, (i + 1.0) / numSamples), samples[i]);
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
    mix.reset (0.0);
    shapeCrossfade.reset (1.0);
}

void Band::setSettings (const BandSettings& settings, bool snap)
{
    const double audible = settings.inUse && ! settings.bypass ? 1.0 : 0.0;
    const auto structure = structureOf (settings);

    // A silent Band has nothing to glide or crossfade from: it takes its new settings at once and fades in.
    if (snap || isSilent())
    {
        logFrequency.reset (std::log (settings.frequency));
        gain.reset (settings.gain);
        logQ.reset (std::log (settings.q));
        current.structure = structure;
        current.clear();
        shapeCrossfade.reset (1.0);
        pending.reset();
        design();
    }
    else
    {
        logFrequency.setTarget (std::log (settings.frequency));
        gain.setTarget (settings.gain);
        logQ.setTarget (std::log (settings.q));
        pending.reset();
        if (structure != current.structure)
        {
            if (shapeCrossfade.isMoving())
            {
                pending = structure;
            }
            else
            {
                crossfadeTo (structure);
            }
        }
    }

    if (snap)
        mix.reset (audible);
    else
        mix.setTarget (audible);
}

// The old filter keeps playing while the new one fades in. (Swapping moves the per-channel state
// without allocating.)
void Band::crossfadeTo (const Structure& structure)
{
    std::swap (current, previous);
    current.structure = structure;
    current.clear();
    design();
    shapeCrossfade.reset (0.0);
    shapeCrossfade.setTarget (1.0);
}

void Band::design()
{
    current.cascade = designShape ({ current.structure, std::exp (logFrequency.value()), gain.value(), std::exp (logQ.value()) },
                                   sampleRate);
}

void Band::process (float* const* channels, int numChannels, int numSamples)
{
    if (isSilent())
        return;

    if (pending && ! shapeCrossfade.isMoving())
    {
        crossfadeTo (*pending);
        pending.reset();
    }

    const bool gliding = logFrequency.isMoving() || gain.isMoving() || logQ.isMoving();
    const bool fading = mix.isMoving();
    const bool crossfading = shapeCrossfade.isMoving();

    const auto from = current.cascade;
    if (gliding)
    {
        logFrequency.skip (numSamples);
        gain.skip (numSamples);
        logQ.skip (numSamples);
        design();
    }

    if (! fading && ! crossfading && mix.value() == 1.0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            current.run (static_cast<size_t> (ch), channels[ch], numSamples, gliding ? &from : nullptr);
        return;
    }

    std::array<double, maxSubBlock> mixes, crossfades;
    for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
    {
        mixes[i] = fading ? mix.next() : mix.value();
        crossfades[i] = crossfading ? shapeCrossfade.next() : shapeCrossfade.value();
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto channel = static_cast<size_t> (ch);
        float* samples = channels[ch];
        std::array<float, maxSubBlock> wet, old;
        std::copy (samples, samples + numSamples, wet.begin());
        current.run (channel, wet.data(), numSamples, gliding ? &from : nullptr);
        if (crossfading)
        {
            std::copy (samples, samples + numSamples, old.begin());
            previous.run (channel, old.data(), numSamples, nullptr);
            for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
                wet[i] = static_cast<float> (old[i] + crossfades[i] * (wet[i] - old[i]));
        }
        for (size_t i = 0; i < static_cast<size_t> (numSamples); ++i)
            samples[i] = static_cast<float> (samples[i] + mixes[i] * (wet[i] - samples[i]));
    }

    // Faded out: start clean when the Band next fades in.
    if (isSilent())
    {
        current.clear();
        previous.clear();
    }
}

} // namespace eq1
