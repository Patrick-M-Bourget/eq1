#include "Band.h"

#include "BellDesign.h"

#include <array>
#include <cmath>

namespace eq1
{

namespace
{
// Glides and crossfades settle in about 7 time constants, so about 50 ms.
constexpr double glideTimeConstantSeconds = 0.007;
}

void Band::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    const double timeConstant = glideTimeConstantSeconds * sampleRate;
    // Close enough to stop gliding: 0.001% in Frequency and Q, 0.0001 dB in Gain, -120 dB in crossfade.
    logFrequency.configure (timeConstant, 1.0e-5);
    gain.configure (timeConstant, 1.0e-4);
    logQ.configure (timeConstant, 1.0e-5);
    mix.configure (timeConstant, 1.0e-6);
    states.assign (static_cast<size_t> (numChannels), {});
    mix.reset (0.0);
}

void Band::setSettings (const BandSettings& settings, bool snap)
{
    const double audible = settings.inUse && ! settings.bypass ? 1.0 : 0.0;

    // A silent Band has nothing to glide from: it takes its new settings at once and fades in.
    if (snap || isSilent())
    {
        logFrequency.reset (std::log (settings.frequency));
        gain.reset (settings.gain);
        logQ.reset (std::log (settings.q));
        designCoefficients();
    }
    else
    {
        logFrequency.setTarget (std::log (settings.frequency));
        gain.setTarget (settings.gain);
        logQ.setTarget (std::log (settings.q));
    }

    if (snap)
        mix.reset (audible);
    else
        mix.setTarget (audible);
}

void Band::designCoefficients()
{
    coefficients = designBell (sampleRate, std::exp (logFrequency.value()), gain.value(), std::exp (logQ.value()));
}

void Band::process (float* const* channels, int numChannels, int numSamples)
{
    if (isSilent())
        return;

    const bool gliding = logFrequency.isMoving() || gain.isMoving() || logQ.isMoving();
    const bool fading = mix.isMoving();

    if (! gliding && ! fading)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* samples = channels[ch];
            auto& state = states[static_cast<size_t> (ch)];
            for (int i = 0; i < numSamples; ++i)
                samples[i] = state.process (coefficients, samples[i]);
        }
        return;
    }

    // Coefficients are designed once per sub-block and interpolated sample by sample in between,
    // so a glide moves the filter smoothly without designing it at every sample.
    std::array<BiquadCoefficients, maxSubBlock> perSample;
    std::array<double, maxSubBlock> mixes;
    const auto from = coefficients;
    if (gliding)
    {
        logFrequency.skip (numSamples);
        gain.skip (numSamples);
        logQ.skip (numSamples);
        designCoefficients();
    }
    for (int i = 0; i < numSamples; ++i)
    {
        const auto n = static_cast<size_t> (i);
        perSample[n] = gliding ? interpolate (from, coefficients, (i + 1.0) / numSamples) : coefficients;
        mixes[n] = fading ? mix.next() : mix.value();
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* samples = channels[ch];
        auto& state = states[static_cast<size_t> (ch)];
        for (int i = 0; i < numSamples; ++i)
        {
            const auto n = static_cast<size_t> (i);
            const float dry = samples[i];
            const float wet = state.process (perSample[n], dry);
            samples[i] = static_cast<float> (dry + mixes[n] * (wet - dry));
        }
    }

    // Faded out: start clean when the Band next fades in.
    if (isSilent())
        for (auto& state : states)
            state = {};
}

} // namespace eq1
