#include "eq1/Engine.h"

#include "AnalysisFifo.h"
#include "Band.h"
#include "LatestValue.h"

#include <algorithm>
#include <array>
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
    std::vector<float*> subBlock; // per channel

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
            bands[band].setSettings (settings.bands[band], snapToSettings);
        settingsChanged = false;
        snapToSettings = false;
    }
};

Engine::Engine() : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int, int numChannels)
{
    impl->numChannels = numChannels;
    for (auto& band : impl->bands)
        band.prepare (sampleRate, numChannels);
    impl->subBlock.assign (static_cast<size_t> (numChannels), nullptr);
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

    const int channels = std::min (main.numChannels, impl->numChannels);
    Impl::pushMonoMix (impl->preEq, main.channels, channels, main.numSamples);
    if (sidechain != nullptr)
        Impl::pushMonoMix (impl->sidechain, sidechain->channels, sidechain->numChannels, sidechain->numSamples);

    for (int start = 0; start < main.numSamples; start += Band::maxSubBlock)
    {
        const int count = std::min (Band::maxSubBlock, main.numSamples - start);
        for (int ch = 0; ch < channels; ++ch)
            impl->subBlock[static_cast<size_t> (ch)] = main.channels[ch] + start;
        for (auto& band : impl->bands)
            band.process (impl->subBlock.data(), channels, count);
    }

    Impl::pushMonoMix (impl->postEq, main.channels, channels, main.numSamples);
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
