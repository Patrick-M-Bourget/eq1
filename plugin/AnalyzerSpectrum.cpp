#include "AnalyzerSpectrum.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace eq1
{

namespace
{
// Time constants of the smoothing, in seconds.
double timeConstantOf (AnalyzerSpeed speed)
{
    switch (speed)
    {
        case AnalyzerSpeed::verySlow: return 2.0;
        case AnalyzerSpeed::slow: return 0.8;
        case AnalyzerSpeed::medium: return 0.3;
        case AnalyzerSpeed::fast: return 0.1;
        case AnalyzerSpeed::veryFast: return 0.03;
    }
    return 0.3;
}

constexpr double silence = 1.0e-20; // -200 dB
} // namespace

int AnalyzerSpectrum::fftSize (double sampleRate, AnalyzerResolution resolution)
{
    const int scale = sampleRate <= 50000.0 ? 1 : sampleRate <= 100000.0 ? 2 : 4;
    return (1024 << static_cast<int> (resolution)) * scale;
}

void AnalyzerSpectrum::prepare (double newSampleRate, AnalyzerResolution newResolution)
{
    sampleRate = newSampleRate;
    resolution = newResolution;
    size = fftSize (sampleRate, resolution);
    fft = std::make_unique<juce::dsp::FFT> (static_cast<int> (std::log2 (size)));

    window.resize (static_cast<size_t> (size));
    double sum = 0.0;
    for (size_t i = 0; i < window.size(); ++i)
    {
        window[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * static_cast<double> (i) / size));
        sum += window[i];
    }
    // A sine of amplitude A peaks at A x sum / 2 in its bin.
    windowGain = sum / 2.0;

    history.assign (static_cast<size_t> (size), 0.0f);
    historyWrite = 0;
    transform.assign (static_cast<size_t> (2 * size), 0.0f);
    power.assign (static_cast<size_t> (size / 2 + 1), silence);
}

void AnalyzerSpectrum::push (const float* samples, int count)
{
    for (int i = std::max (0, count - size); i < count; ++i)
    {
        history[historyWrite] = samples[i];
        historyWrite = (historyWrite + 1) % history.size();
    }
}

void AnalyzerSpectrum::update (double seconds, AnalyzerSpeed speed)
{
    if (fft == nullptr)
        return;
    // The newest size samples, oldest first, windowed.
    for (size_t i = 0; i < history.size(); ++i)
        transform[i] = history[(historyWrite + i) % history.size()] * window[i];
    std::fill (transform.begin() + size, transform.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (transform.data(), true);

    const double amount = 1.0 - std::exp (-seconds / timeConstantOf (speed));
    for (size_t bin = 0; bin < power.size(); ++bin)
    {
        const double amplitude = transform[bin] / windowGain;
        power[bin] += (std::max (amplitude * amplitude, silence) - power[bin]) * amount;
    }
}

double AnalyzerSpectrum::levelDb (double frequency, double tiltDbPerOctave) const
{
    double shown = silence;
    if (! power.empty())
    {
        const double bin = std::clamp (binOf (frequency), 0.0, static_cast<double> (power.size() - 1));
        const auto below = static_cast<size_t> (bin);
        const auto above = std::min (below + 1, power.size() - 1);
        const double fraction = bin - static_cast<double> (below);
        shown = power[below] + (power[above] - power[below]) * fraction;
    }
    return 10.0 * std::log10 (shown) + tiltDbPerOctave * std::log2 (frequency / 1000.0);
}

double AnalyzerSpectrum::peakNear (double frequency) const
{
    if (power.size() < 3)
        return frequency;
    const double sixth = std::exp2 (1.0 / 6.0);
    const auto first = static_cast<size_t> (std::clamp (std::floor (binOf (frequency / sixth)), 1.0, static_cast<double> (power.size() - 2)));
    const auto last = static_cast<size_t> (std::clamp (std::ceil (binOf (frequency * sixth)), static_cast<double> (first), static_cast<double> (power.size() - 2)));
    size_t top = first;
    for (size_t bin = first; bin <= last; ++bin)
        if (power[bin] > power[top])
            top = bin;
    // The highest point is only a peak if it is higher than both its neighbours.
    if (power[top] <= power[top - 1] || power[top] <= power[top + 1])
        return frequency;

    // A parabola through the top bin and its neighbours, in dB, places the peak between bins.
    const double left = 10.0 * std::log10 (power[top - 1]), centre = 10.0 * std::log10 (power[top]),
                 right = 10.0 * std::log10 (power[top + 1]);
    const double curvature = left - 2.0 * centre + right;
    const double offset = curvature < 0.0 ? 0.5 * (left - right) / curvature : 0.0;
    return (static_cast<double> (top) + std::clamp (offset, -0.5, 0.5)) * sampleRate / size;
}

} // namespace eq1
