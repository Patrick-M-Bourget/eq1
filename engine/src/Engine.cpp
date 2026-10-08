#include "eq1/Engine.h"

#include "AnalysisFifo.h"
#include "Band.h"
#include "Dynamics.h"
#include "LatestValue.h"
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
    std::vector<float*> subBlock; // per channel

    // Solo: the region filter, run on the part of the input the Soloed Band processes, and the
    // crossfade between the EQ's output (0) and the region (1).
    static constexpr double soloFadeTimeConstantSeconds = 0.007; // as a Band's crossfades: about 50 ms
    Band soloRegion;
    Smoother soloMix;
    int soloSlot = 0; // the slot soloRegion is set up for
    StereoPlacement soloPlacement = StereoPlacement::Stereo;
    // The Solo the settings ask for. A different one than is playing waits for that one to fade out.
    int wantedSoloSlot = 0;
    StereoPlacement wantedSoloPlacement = StereoPlacement::Stereo;
    BandSettings wantedSoloRegion;
    std::vector<std::array<float, Band::maxSubBlock>> soloPart; // per channel
    std::vector<float*> soloPartChannels;

    bool soloAudible() const { return soloMix.value() > 0.0 || soloMix.isMoving(); }

    // The part of the sub-block the Soloed Band processes, into soloPart. Returns how many channels
    // it has: all of them for Stereo, one for Left, Right, Mid and Side, none for Side on mono.
    int takeSoloPart (float* const* channels, int channelCount, int count)
    {
        const auto copy = [&] (const float* from) { std::copy (from, from + count, soloPart[0].data()); };
        if (channelCount < 2)
        {
            if (soloPlacement == StereoPlacement::Side)
                return 0;
            copy (channels[0]);
            return 1;
        }
        switch (soloPlacement)
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
                const float sign = soloPlacement == StereoPlacement::Mid ? 1.0f : -1.0f;
                for (int i = 0; i < count; ++i)
                    soloPart[0][static_cast<size_t> (i)] = 0.5f * (channels[0][i] + sign * channels[1][i]);
                return 1;
            }
        }
        return 0;
    }

    // What Solo plays on output channel ch at sample i: the region, decoded back to where it came from.
    float soloSample (int partChannels, int ch, int i) const
    {
        const auto at = [&] (int c) { return soloPart[static_cast<size_t> (c)][static_cast<size_t> (i)]; };
        if (partChannels == 0)
            return 0.0f;
        switch (soloPlacement)
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
            bands[band].setSettings (settings.bands[band], snapToSettings);
            dynamics[band].setSettings (settings.bands[band], snapToSettings);
        }

        const int slot = settings.soloSlot;
        wantedSoloSlot = slot >= 1 && slot <= numBandSlots && settings.bands[static_cast<size_t> (slot - 1)].inUse ? slot : 0;
        if (wantedSoloSlot != 0)
        {
            const auto& band = settings.bands[static_cast<size_t> (wantedSoloSlot - 1)];
            wantedSoloPlacement = band.placement;
            wantedSoloRegion = soloRegionOf (band);
            // The playing Solo follows its Band's edits.
            if (isPlayingWantedSolo() && ! snapToSettings)
                soloRegion.setSettings (wantedSoloRegion, false);
        }
        if (snapToSettings)
        {
            soloMix.reset (0.0);
            startWantedSolo (true);
        }
        settingsChanged = false;
        snapToSettings = false;
    }

    bool isPlayingWantedSolo() const { return wantedSoloSlot == soloSlot && wantedSoloPlacement == soloPlacement; }

    // Starts playing the wanted Solo, once nothing else is audible; with snap, at full level at once.
    void startWantedSolo (bool snap)
    {
        soloSlot = wantedSoloSlot;
        soloPlacement = wantedSoloPlacement;
        if (soloSlot == 0)
            return;
        // A newly Soloed Band's region starts at once; the crossfade to it is soloMix's.
        soloRegion.setSettings (wantedSoloRegion, true);
        if (snap)
            soloMix.reset (1.0);
        else
            soloMix.setTarget (1.0);
    }

    // Once a block: fades in the wanted Solo, or fades out a different one first, so moving Solo to
    // another Band, or changing the Soloed Band's Stereo Placement, crossfades through the EQ's output.
    void updateSolo()
    {
        if (wantedSoloSlot != 0 && isPlayingWantedSolo())
            soloMix.setTarget (1.0);
        else if (soloAudible())
            soloMix.setTarget (0.0);
        else
            startWantedSolo (false);
    }
};

Engine::Engine() : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int, int numChannels)
{
    impl->numChannels = numChannels;
    for (auto& band : impl->bands)
        band.prepare (sampleRate, numChannels);
    for (auto& dynamics : impl->dynamics)
        dynamics.prepare (sampleRate);
    impl->subBlock.assign (static_cast<size_t> (numChannels), nullptr);
    impl->soloRegion.prepare (sampleRate, numChannels);
    impl->soloMix.configure (Impl::soloFadeTimeConstantSeconds * sampleRate, 1.0e-6);
    impl->soloMix.reset (0.0);
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
    impl->updateSolo();

    const int channels = std::min (main.numChannels, impl->numChannels);
    Impl::pushMonoMix (impl->preEq, main.channels, channels, main.numSamples);
    if (sidechain != nullptr)
        Impl::pushMonoMix (impl->sidechain, sidechain->channels, sidechain->numChannels, sidechain->numSamples);

    for (int start = 0; start < main.numSamples; start += Band::maxSubBlock)
    {
        const int count = std::min (Band::maxSubBlock, main.numSamples - start);
        for (int ch = 0; ch < channels; ++ch)
            impl->subBlock[static_cast<size_t> (ch)] = main.channels[ch] + start;

        const bool soloing = impl->soloAudible();
        const int partChannels = soloing ? impl->takeSoloPart (impl->subBlock.data(), channels, count) : 0;

        // Detection hears the main input before the EQ, so every Band's detector runs first.
        for (size_t band = 0; band < impl->bands.size(); ++band)
            impl->bands[band].setDynamicOffset (impl->dynamics[band].process (impl->subBlock.data(), channels, count));
        for (auto& band : impl->bands)
            band.process (impl->subBlock.data(), channels, count);

        if (soloing)
        {
            if (partChannels > 0)
                impl->soloRegion.process (impl->soloPartChannels.data(), partChannels, count);
            std::array<double, Band::maxSubBlock> mixes;
            for (size_t i = 0; i < static_cast<size_t> (count); ++i)
                mixes[i] = impl->soloMix.next();
            for (int ch = 0; ch < channels; ++ch)
            {
                float* samples = impl->subBlock[static_cast<size_t> (ch)];
                for (int i = 0; i < count; ++i)
                {
                    // In double, so a full mix plays the region exactly.
                    const double dry = samples[i];
                    samples[i] = static_cast<float> (dry + mixes[static_cast<size_t> (i)] * (impl->soloSample (partChannels, ch, i) - dry));
                }
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
