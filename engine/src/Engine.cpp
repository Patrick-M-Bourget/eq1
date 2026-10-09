#include "eq1/Engine.h"

#include "AnalysisFifo.h"
#include "AutoGain.h"
#include "Band.h"
#include "Dynamics.h"
#include "LatestValue.h"
#include "Output.h"
#include "Smoother.h"
#include "Solo.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

namespace eq1
{

struct Engine::Impl
{
    int numChannels = 0;

    LatestValue<Settings> handoff;
    Settings settings;          // the newest settings the audio thread has taken
    bool settingsChanged = false;
    bool snapToSettings = true; // after prepare, settings apply at once instead of gliding

    std::array<Band, numBandSlots> bands;
    std::array<Dynamics, numBandSlots> dynamics;
    std::array<std::atomic<double>, numBandSlots> liveGains {}; // for the display, after each block
    static_assert (std::atomic<double>::is_always_lock_free, "process() never locks");
    Output output;

    // Global Bypass: bypassMix crossfades from the processed output (0) to the input (1), copied into dry.
    static constexpr double bypassFadeTimeConstantSeconds = 0.007; // as a Band's crossfades: about 50 ms
    Smoother bypassMix;
    std::vector<std::array<float, Band::maxSubBlock>> dry; // per channel

    // Auto Gain's estimate is worked out over several blocks, at most a point every
    // samplesPerAutoGainPoint samples, so a new estimate takes about 40 ms at 48 kHz.
    static constexpr int samplesPerAutoGainPoint = 2;
    double sampleRate = 48000.0;
    AutoGainEstimate autoGain;
    Settings estimated; // the settings of the estimate running or last finished: only Bands and Gain Scale count
    bool estimateStale = false;
    std::vector<float*> subBlock; // per channel
    std::array<const float*, 2> sidechainSubBlock {}; // the Sidechain's first two channels

    // What plays instead of the EQ's output while the editor holds it: a Band's Solo, or its Detection
    // Audition. Changing any of it fades the old one out before the new one fades in.
    struct Held
    {
        int slot = 0; // 0 for none
        bool audition = false;
        StereoPlacement placement = StereoPlacement::Stereo;
        DetectionSource source = DetectionSource::Internal; // Detection Audition only: switching it crossfades

        bool operator== (const Held&) const = default;
    };

    // Solo: the region filter, run on the part of the input the Soloed Band processes. Detection
    // Audition: the auditioned Band's detection signal. heldMix crossfades between the EQ's output
    // (0) and them (1).
    static constexpr double heldFadeTimeConstantSeconds = 0.007; // as a Band's crossfades: about 50 ms
    Band soloRegion;
    Smoother heldMix;
    Held playing; // what heldMix fades in, and soloRegion is set up for
    // What the settings ask for. A different one than is playing waits for that one to fade out.
    Held wanted;
    BandSettings wantedSoloRegion;
    std::vector<std::array<float, Band::maxSubBlock>> soloPart; // per channel
    std::vector<float*> soloPartChannels;

    bool heldAudible() const { return heldMix.value() > 0.0 || heldMix.isMoving(); }

    // The part of the sub-block the Soloed Band processes, into soloPart. Returns how many channels
    // it has: all of them for Stereo, one for Left, Right, Mid and Side, none for Side on mono.
    int takeSoloPart (float* const* channels, int channelCount, int count)
    {
        const auto copy = [&] (const float* from) { std::copy (from, from + count, soloPart[0].data()); };
        if (channelCount < 2)
        {
            if (playing.placement == StereoPlacement::Side)
                return 0;
            copy (channels[0]);
            return 1;
        }
        switch (playing.placement)
        {
            case StereoPlacement::Stereo:
                for (int ch = 0; ch < channelCount; ++ch)
                    std::copy (channels[ch], channels[ch] + count, soloPart[static_cast<size_t> (ch)].data());
                return channelCount;
            case StereoPlacement::Left: copy (channels[0]); return 1;
            case StereoPlacement::Right: copy (channels[1]); return 1;
            case StereoPlacement::Mid:
            case StereoPlacement::Side:
            {
                const float sign = playing.placement == StereoPlacement::Mid ? 1.0f : -1.0f;
                for (int i = 0; i < count; ++i)
                    soloPart[0][static_cast<size_t> (i)] = 0.5f * (channels[0][i] + sign * channels[1][i]);
                return 1;
            }
        }
        return 0;
    }

    // What plays on output channel ch at sample i: for Solo, the region, decoded back to where it came
    // from; for Detection Audition, the detection signal.
    float heldSample (int partChannels, int ch, int i) const
    {
        if (playing.audition)
            return dynamics[static_cast<size_t> (playing.slot - 1)].auditionSample (numChannels, ch, i);
        const auto at = [&] (int c) { return soloPart[static_cast<size_t> (c)][static_cast<size_t> (i)]; };
        if (partChannels == 0)
            return 0.0f;
        switch (playing.placement)
        {
            case StereoPlacement::Stereo: return at (ch);
            case StereoPlacement::Left: return numChannels < 2 || ch == 0 ? at (0) : 0.0f;
            case StereoPlacement::Right: return numChannels < 2 || ch == 1 ? at (0) : 0.0f;
            case StereoPlacement::Mid: return ch < 2 ? at (0) : 0.0f;
            case StereoPlacement::Side: return ch == 0 ? at (0) : ch == 1 ? -at (0) : 0.0f;
        }
        return 0.0f;
    }

    // About 1.4 s at 48 kHz: enough for the Analyzer to read at display rate.
    static constexpr int analysisCapacity = 1 << 16;
    AnalysisFifo preEq, postEq, sidechain;

    static void pushMonoMix (AnalysisFifo& fifo, const float* const* channels, int numChannels, int numSamples)
    {
        if (numChannels <= 0)
            return;
        const float scale = 1.0f / static_cast<float> (numChannels);
        fifo.push (numSamples, [&] (int i) {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += channels[ch][i];
            return sum * scale;
        });
    }

    void applySettings()
    {
        if (const auto* latest = handoff.takeLatest())
        {
            settings = *latest;
            settingsChanged = true;
        }
        if (! settingsChanged)
            return;
        for (size_t band = 0; band < bands.size(); ++band)
        {
            const auto playing = scaledByGainScale (settings.bands[band], settings.gainScale);
            bands[band].setSettings (playing, snapToSettings);
            dynamics[band].setSettings (playing, snapToSettings);
        }
        if (snapToSettings)
        {
            // The first estimate is worked out at once, so the output starts at the right level.
            estimated = settings;
            autoGain.start (settings, sampleRate);
            autoGain.advance (AutoGainEstimate::numPoints);
            estimateStale = false;
        }
        else if (settings.bands != estimated.bands || settings.gainScale != estimated.gainScale)
        {
            estimateStale = true;
        }
        updateOutput (snapToSettings);
        if (snapToSettings)
            bypassMix.reset (settings.globalBypass ? 1.0 : 0.0);
        else
            bypassMix.setTarget (settings.globalBypass ? 1.0 : 0.0);

        const auto bandIn = [&] (int slot) -> const BandSettings* {
            const bool valid = slot >= 1 && slot <= numBandSlots && settings.bands[static_cast<size_t> (slot - 1)].inUse;
            return valid ? &settings.bands[static_cast<size_t> (slot - 1)] : nullptr;
        };
        wanted = {};
        if (const auto* band = bandIn (settings.auditionSlot); band != nullptr && hasDynamics (band->shape))
        {
            wanted = { .slot = settings.auditionSlot, .audition = true, .placement = band->placement, .source = band->detectionSource };
        }
        else if (const auto* soloed = bandIn (settings.soloSlot))
        {
            wanted = { .slot = settings.soloSlot, .placement = soloed->placement };
            wantedSoloRegion = soloRegionOf (*soloed);
            // The playing Solo follows its Band's edits.
            if (playing == wanted && ! snapToSettings)
                soloRegion.setSettings (wantedSoloRegion, false);
        }
        if (snapToSettings)
        {
            heldMix.reset (0.0);
            startWantedHeld (true);
        }
        settingsChanged = false;
        snapToSettings = false;
    }

    void updateOutput (bool snap) { output.setSettings (settings, settings.autoGain ? autoGain.db() : 0.0, snap); }

    // Once a block: carries on with the Auto Gain estimate, starting a new one once the last has
    // finished if the Bands have changed since, and glides the output to it when it finishes.
    void updateAutoGain (int numSamples)
    {
        if (! autoGain.running())
        {
            if (! estimateStale)
                return;
            estimated = settings;
            autoGain.start (settings, sampleRate);
            estimateStale = false;
        }
        if (autoGain.advance ((numSamples + samplesPerAutoGainPoint - 1) / samplesPerAutoGainPoint))
            updateOutput (false);
    }

    // Starts playing the wanted Solo or Detection Audition, once nothing else is audible; with snap,
    // at full level at once.
    void startWantedHeld (bool snap)
    {
        playing = wanted;
        if (playing.slot == 0)
            return;
        // A newly Soloed Band's region starts at once; the crossfade to it is heldMix's.
        if (! playing.audition)
            soloRegion.setSettings (wantedSoloRegion, true);
        if (snap)
            heldMix.reset (1.0);
        else
            heldMix.setTarget (1.0);
    }

    // Once a block: fades in the wanted Solo or Detection Audition, or fades out a different one
    // first, so moving Solo to another Band, or changing the Soloed Band's Stereo Placement,
    // crossfades through the EQ's output. An auditioned Band's detector runs until it has faded out.
    void updateHeld()
    {
        if (wanted.slot != 0 && playing == wanted)
            heldMix.setTarget (1.0);
        else if (heldAudible())
            heldMix.setTarget (0.0);
        else
            startWantedHeld (false);
        for (size_t band = 0; band < dynamics.size(); ++band)
            dynamics[band].setAuditioned (playing.audition && playing.slot == static_cast<int> (band) + 1 && heldAudible());
    }
};

Engine::Engine() : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int, int numChannels)
{
    impl->numChannels = numChannels;
    impl->sampleRate = sampleRate;
    for (auto& band : impl->bands)
        band.prepare (sampleRate, numChannels);
    for (auto& dynamics : impl->dynamics)
        dynamics.prepare (sampleRate);
    impl->output.prepare (sampleRate, numChannels);
    impl->bypassMix.configure (Impl::bypassFadeTimeConstantSeconds * sampleRate, 1.0e-6);
    impl->bypassMix.reset (0.0);
    impl->dry.assign (static_cast<size_t> (numChannels), {});
    impl->subBlock.assign (static_cast<size_t> (numChannels), nullptr);
    impl->soloRegion.prepare (sampleRate, numChannels);
    impl->heldMix.configure (Impl::heldFadeTimeConstantSeconds * sampleRate, 1.0e-6);
    impl->heldMix.reset (0.0);
    impl->soloPart.assign (static_cast<size_t> (std::max (numChannels, 1)), {});
    impl->soloPartChannels.clear();
    for (auto& part : impl->soloPart)
        impl->soloPartChannels.push_back (part.data());
    impl->settingsChanged = true;
    impl->snapToSettings = true;
    impl->preEq.allocate (Impl::analysisCapacity);
    impl->postEq.allocate (Impl::analysisCapacity);
    impl->sidechain.allocate (Impl::analysisCapacity);
}

void Engine::setSettings (const Settings& settings)
{
    impl->handoff.publish (settings);
}

void Engine::process (AudioBlock main, const ConstAudioBlock* sidechain)
{
    impl->applySettings();
    impl->updateHeld();
    impl->updateAutoGain (main.numSamples);

    const int channels = std::min (main.numChannels, impl->numChannels);
    Impl::pushMonoMix (impl->preEq, main.channels, channels, main.numSamples);
    // A Sidechain without channels, or shorter than the block, counts as none connected.
    const bool sidechainConnected = sidechain != nullptr && sidechain->numChannels > 0 && sidechain->numSamples >= main.numSamples;
    if (sidechainConnected)
        Impl::pushMonoMix (impl->sidechain, sidechain->channels, sidechain->numChannels, main.numSamples);
    const int sidechainChannels = sidechainConnected ? std::min (sidechain->numChannels, 2) : 0;

    for (int start = 0; start < main.numSamples; start += Band::maxSubBlock)
    {
        const int count = std::min (Band::maxSubBlock, main.numSamples - start);
        for (int ch = 0; ch < channels; ++ch)
            impl->subBlock[static_cast<size_t> (ch)] = main.channels[ch] + start;
        for (int ch = 0; ch < sidechainChannels; ++ch)
            impl->sidechainSubBlock[static_cast<size_t> (ch)] = sidechain->channels[ch] + start;

        const bool bypassing = impl->bypassMix.value() > 0.0 || impl->bypassMix.isMoving();
        if (bypassing)
            for (int ch = 0; ch < channels; ++ch)
                std::copy (impl->subBlock[static_cast<size_t> (ch)], impl->subBlock[static_cast<size_t> (ch)] + count,
                           impl->dry[static_cast<size_t> (ch)].data());

        const bool holding = impl->heldAudible();
        const int partChannels = holding && ! impl->playing.audition ? impl->takeSoloPart (impl->subBlock.data(), channels, count) : 0;

        // Detection hears the main input before the EQ (or the Sidechain), so every Band's detector runs first.
        for (size_t band = 0; band < impl->bands.size(); ++band)
            impl->bands[band].setDynamicOffset (impl->dynamics[band].process (impl->subBlock.data(), channels,
                                                                              impl->sidechainSubBlock.data(), sidechainChannels, count));
        for (auto& band : impl->bands)
            band.process (impl->subBlock.data(), channels, count);

        if (holding)
        {
            if (partChannels > 0)
                impl->soloRegion.process (impl->soloPartChannels.data(), partChannels, count);
            std::array<double, Band::maxSubBlock> mixes;
            for (size_t i = 0; i < static_cast<size_t> (count); ++i)
                mixes[i] = impl->heldMix.next();
            for (int ch = 0; ch < channels; ++ch)
            {
                float* samples = impl->subBlock[static_cast<size_t> (ch)];
                for (int i = 0; i < count; ++i)
                {
                    // In double, so a full mix plays the region exactly.
                    const double dry = samples[i];
                    samples[i] = static_cast<float> (dry + mixes[static_cast<size_t> (i)] * (impl->heldSample (partChannels, ch, i) - dry));
                }
            }
        }

        impl->output.process (impl->subBlock.data(), channels, count);

        if (bypassing)
        {
            std::array<double, Band::maxSubBlock> mixes;
            for (size_t i = 0; i < static_cast<size_t> (count); ++i)
                mixes[i] = impl->bypassMix.next();
            for (int ch = 0; ch < channels; ++ch)
            {
                float* samples = impl->subBlock[static_cast<size_t> (ch)];
                const auto& input = impl->dry[static_cast<size_t> (ch)];
                for (size_t i = 0; i < static_cast<size_t> (count); ++i)
                    samples[i] = static_cast<float> (samples[i] + mixes[i] * (input[i] - samples[i]));
            }
        }
    }

    Impl::pushMonoMix (impl->postEq, main.channels, channels, main.numSamples);
    for (size_t band = 0; band < impl->bands.size(); ++band)
        impl->liveGains[band].store (impl->bands[band].liveGainDb(), std::memory_order_relaxed);
}

double Engine::liveGainDb (int slot) const
{
    return slot >= 1 && slot <= numBandSlots ? impl->liveGains[static_cast<size_t> (slot - 1)].load (std::memory_order_relaxed) : 0.0;
}

int Engine::readAnalysis (AnalysisTap tap, float* destination, int maxSamples)
{
    switch (tap)
    {
        case AnalysisTap::PreEq: return impl->preEq.pop (destination, maxSamples);
        case AnalysisTap::PostEq: return impl->postEq.pop (destination, maxSamples);
        case AnalysisTap::Sidechain: return impl->sidechain.pop (destination, maxSamples);
    }
    return 0;
}

} // namespace eq1
