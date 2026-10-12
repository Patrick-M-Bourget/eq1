#include "Band.h"

#include "Crossfade.h"
#include "Placement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <utility>

namespace eq1
{

void Band::Chain::clear()
{
    for (auto& channel : states)
        channel = {};
}

bool Band::Chain::clearNonFinite()
{
    bool cleared = false;
    for (auto& channel : states)
    {
        for (size_t s = 0; s < static_cast<size_t> (cascade.count); ++s)
        {
            // One check a section: a sum of two finite numbers is finite, and one NaN or infinity makes it not.
            auto& state = channel[s];
            if (std::isfinite (state.s1 + state.s2))
                continue;
            state = {};
            cleared = true;
        }
    }
    return cleared;
}

void Band::Chain::Lines::prepare (int numChannels)
{
    const auto size = static_cast<size_t> (numChannels);
    samples.assign (size, nullptr);
    states.assign (size, 0);
    count = 0;
}

void Band::Chain::run (Lines& lines, const Piece& piece)
{
    const auto numLines = static_cast<size_t> (lines.count);
    const auto numSamples = static_cast<size_t> (piece.numSamples);
    for (size_t s = 0; s < static_cast<size_t> (cascade.count); ++s)
    {
        const auto& to = cascade.sections[s];
        if (piece.from == nullptr)
        {
            for (size_t line = 0; line < numLines; ++line)
            {
                auto& state = states[lines.states[line]][s];
                for (auto& sample : std::span (lines.samples[line], numSamples))
                    sample = state.process (to, sample);
            }
            continue;
        }
        // Designed once per run and interpolated by position in it, once per position for every line.
        const auto& start = piece.from->sections[s];
        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto coefficients = interpolate (start, to, (piece.position + static_cast<double> (i) + 1.0) / maxSubBlock);
            for (size_t line = 0; line < numLines; ++line)
                lines.samples[line][i] = states[lines.states[line]][s].process (coefficients, lines.samples[line][i]);
        }
    }
}

void Band::Chain::runPlaced (float* const* channels, int numChannels, Lines& lines, const Piece& piece)
{
    lines.count = 0;
    std::optional<bool> midSide;
    routePlacement (
        setup.placement, numChannels,
        [&] (int part, int ch) {
            lines.samples[static_cast<size_t> (part)] = channels[ch];
            lines.states[static_cast<size_t> (part)] = static_cast<size_t> (ch);
            lines.count = part + 1;
        },
        [&] (bool mid) { midSide = mid; });
    if (midSide)
        runMidSide (channels, lines, piece, *midSide);
    else if (lines.count > 0)
        run (lines, piece);
}

void Band::Chain::runMidSide (float* const* channels, Lines& lines, const Piece& piece, bool mid)
{
    std::array<float, maxSubBlock> midSamples, sideSamples;
    float* left = channels[0];
    float* right = channels[1];
    const auto numSamples = static_cast<size_t> (piece.numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        midSamples[i] = encodeMid (left[i], right[i]);
        sideSamples[i] = encodeSide (left[i], right[i]);
    }
    lines.samples[0] = mid ? midSamples.data() : sideSamples.data();
    lines.states[0] = 0;
    lines.count = 1;
    run (lines, piece);
    for (size_t i = 0; i < numSamples; ++i)
    {
        left[i] = decodeLeft (midSamples[i], sideSamples[i]);
        right[i] = decodeRight (midSamples[i], sideSamples[i]);
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
    lines.prepare (numChannels);
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
    startedRun = noRun;
    // A Dynamic Band starts the new session at its Gain, as its detector does.
    dynamicOffset = designedOffset = 0.0;
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

void Band::process (float* const* channels, int numChannels, std::uint64_t run, int position, int numSamples)
{
    const bool joinedLate = position != 0 && run != startedRun;
    runPosition = position + numSamples;
    // A Band silent at the run's start still saw it, so it hasn't joined late if it sounds later on.
    if (position == 0)
        startedRun = run;
    if (isSilent())
        return;

    if (position == 0)
    {
        // A chain a non-finite sample got into starts afresh at this run's filter, with no glide from
        // the one it lost: the output is finite again from here, and its detector's offset, which
        // starts afresh too, applies at once.
        bool recovering = current.clearNonFinite();
        if (shapeCrossfade.isMoving() && previous.clearNonFinite())
            recovering = true;
        startRun();
        if (recovering)
        {
            runGliding = false;
            runStartGainDb = runEndGainDb;
        }
    }
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
        current.runPlaced (channels, numChannels, lines, piece);
        return;
    }

    // A channel the chains leave alone comes out of the mix exactly as it went in.
    const auto copyInput = [&] (std::vector<float*>& to) {
        for (int ch = 0; ch < numChannels; ++ch)
            std::copy (channels[ch], channels[ch] + numSamples, to[static_cast<size_t> (ch)]);
    };
    const auto sampleOf = [] (const std::vector<std::array<float, maxSubBlock>>& buffers, int ch, int i) {
        return buffers[static_cast<size_t> (ch)][static_cast<size_t> (i)];
    };
    copyInput (wetChannels);
    current.runPlaced (wetChannels.data(), numChannels, lines, piece);
    // The filtered signal the dry one mixes with: the current chain's, or while crossfading, the
    // previous chain's moving to it (left in previousWet).
    const auto* filtered = &wet;
    if (crossfading)
    {
        copyInput (previousWetChannels);
        previous.runPlaced (previousWetChannels.data(), numChannels, lines, { position, numSamples });
        crossfade<float> (shapeCrossfade, previousWetChannels.data(), numChannels, numSamples, [&] (int ch, int i) { return sampleOf (wet, ch, i); });
        filtered = &previousWet;
    }
    crossfade<float> (mix, channels, numChannels, numSamples, [&] (int ch, int i) { return sampleOf (*filtered, ch, i); });

    // Faded out: start clean when the Band next fades in.
    if (isSilent())
    {
        current.clear();
        previous.clear();
    }
}

} // namespace eq1
