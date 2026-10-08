#include "eq1/Engine.h"

#include "AnalysisFifo.h"
#include "BellDesign.h"
#include "Biquad.h"

#include <algorithm>
#include <array>
#include <vector>

namespace eq1
{

struct Engine::Impl
{
    double sampleRate = 44100.0;
    int numChannels = 0;
    Settings settings;

    std::array<BiquadCoefficients, numBandSlots> coefficients {};
    std::vector<std::array<BiquadState, numBandSlots>> states; // per channel

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

    void updateCoefficients()
    {
        for (size_t band = 0; band < settings.bands.size(); ++band)
        {
            const auto& b = settings.bands[band];
            coefficients[band] = b.inUse ? designBell (sampleRate, b.frequency, b.gain, b.q) : BiquadCoefficients {};
        }
    }
};

Engine::Engine() : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int, int numChannels)
{
    impl->sampleRate = sampleRate;
    impl->numChannels = numChannels;
    impl->states.assign (static_cast<size_t> (numChannels), {});
    impl->preEq.allocate (Impl::analysisCapacity);
    impl->postEq.allocate (Impl::analysisCapacity);
    impl->sidechain.allocate (Impl::analysisCapacity);
    impl->updateCoefficients();
}

void Engine::setSettings (const Settings& settings)
{
    if (settings == impl->settings)
        return;
    impl->settings = settings;
    impl->updateCoefficients();
}

void Engine::process (AudioBlock main, const ConstAudioBlock* sidechain)
{
    const int channels = std::min (main.numChannels, impl->numChannels);
    Impl::pushMonoMix (impl->preEq, main.channels, channels, main.numSamples);
    if (sidechain != nullptr)
        Impl::pushMonoMix (impl->sidechain, sidechain->channels, sidechain->numChannels, sidechain->numSamples);

    for (int ch = 0; ch < channels; ++ch)
    {
        float* samples = main.channels[ch];
        auto& channelStates = impl->states[static_cast<size_t> (ch)];
        for (size_t band = 0; band < channelStates.size(); ++band)
        {
            if (! impl->settings.bands[band].inUse)
                continue;
            const auto& c = impl->coefficients[band];
            auto& state = channelStates[band];
            for (int i = 0; i < main.numSamples; ++i)
                samples[i] = state.process (c, samples[i]);
        }
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
