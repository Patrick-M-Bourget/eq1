// Measures the Engine's CPU load with 24 Dynamic Bands and fails when it is over budget
// (docs/performance.md, "CPU budget"). Run by scripts/check.sh cpu, on its own: timings taken while
// other tests run are meaningless.

#include "eq1/Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

using namespace eq1;

namespace
{

constexpr int blockSize = 512;
constexpr int runs = 5;
constexpr double secondsPerRun = 4.0;

// 24 Bells across the spectrum, each a Dynamic Band; every Detection Range and Threshold kind, so
// every Band's filter is redesigned as its Live Gain moves.
Settings dynamicBands()
{
    Settings settings;
    for (size_t slot = 0; slot < settings.bands.size(); ++slot)
        settings.bands[slot] = { .inUse = true,
                                 .shape = Shape::Bell,
                                 .frequency = 30.0 * std::pow (1.33, static_cast<double> (slot)),
                                 .gain = 3.0,
                                 .q = 1.0,
                                 .detectionRange = slot % 2 == 0 ? DetectionRange::Band : DetectionRange::Free,
                                 .detectionLow = 100.0,
                                 .detectionHigh = 5000.0,
                                 .dynamicRange = slot % 2 == 0 ? -12.0 : 6.0,
                                 .threshold = -30.0,
                                 .thresholdAuto = slot % 3 == 0 };
    return settings;
}

// What the input carries, as a function of the sample's time in seconds and a noise sample.
using Signal = std::function<float (double seconds, float noise)>;

// Noise that is loud for 100 ms of every 200, so the Bands keep moving.
float bursts (double seconds, float noise) { return std::fmod (seconds, 0.2) < 0.1 ? 0.5f * noise : 0.01f * noise; }

// The Engine's load, in percent of real time: the median of several runs, each in a fresh Engine.
double loadPercent (double sampleRate, const Signal& signal, double startSeconds = 0.0)
{
    std::vector<double> loads;
    std::mt19937 random (1);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    std::vector<float> left (blockSize), right (blockSize);
    float* main[] = { left.data(), right.data() };
    for (int run = 0; run < runs; ++run)
    {
        Engine engine;
        engine.prepare (sampleRate, blockSize, 2);
        engine.setSettings (dynamicBands());
        const auto blocks = static_cast<long> ((startSeconds + secondsPerRun) * sampleRate / blockSize);
        const auto firstTimed = static_cast<long> (startSeconds * sampleRate / blockSize);
        std::chrono::steady_clock::duration busy {};
        for (long block = 0; block < blocks; ++block)
        {
            for (size_t i = 0; i < left.size(); ++i)
            {
                const double seconds = static_cast<double> (block * blockSize + static_cast<long> (i)) / sampleRate;
                left[i] = signal (seconds, noise (random));
                right[i] = signal (seconds, noise (random));
            }
            const auto start = std::chrono::steady_clock::now();
            engine.process ({ main, 2, blockSize });
            if (block >= firstTimed)
                busy += std::chrono::steady_clock::now() - start;
        }
        loads.push_back (100.0 * std::chrono::duration<double> (busy).count() / secondsPerRun);
    }
    std::sort (loads.begin(), loads.end());
    return loads[loads.size() / 2];
}

struct Budget
{
    double sampleRate;
    double ceilingPercent;
};

// docs/performance.md, "CPU budget".
constexpr Budget budgets[] = { { 48000.0, 35.0 }, { 96000.0, 60.0 } };

// Silence and subnormal input may cost no more than this times the music. Subnormal arithmetic is
// many times slower on x64, so a filter left to ring down into subnormal numbers is far over it.
constexpr double quietCostRatio = 1.5;

} // namespace

int main()
{
    bool overBudget = false;
    std::printf ("24 Dynamic Bands, stereo, %d-sample blocks: median of %d runs of %.0f s\n\n", blockSize, runs, secondsPerRun);
    for (const auto& [sampleRate, ceiling] : budgets)
    {
        const double music = loadPercent (sampleRate, bursts);
        // Silence after 10 s of nothing, long enough for every filter to have rung down below the
        // smallest normal float; then input that is all subnormal numbers.
        const double silence = loadPercent (sampleRate, [] (double seconds, float noise) { return seconds < 0.5 ? noise : 0.0f; }, 10.0);
        const double subnormal = loadPercent (sampleRate, [] (double, float noise) { return 1.0e-39f * noise; });

        const bool musicOver = music > ceiling;
        const bool quietOver = std::max (silence, subnormal) > quietCostRatio * music;
        overBudget = overBudget || musicOver || quietOver;
        std::printf ("%6.1f kHz  music %5.2f%% (ceiling %4.1f%%)%s  silence %5.2f%%  subnormal input %5.2f%%", sampleRate / 1000.0, music,
                     ceiling, musicOver ? " OVER" : "", silence, subnormal);
        if (quietOver)
            std::printf (" OVER: more than %.1f x music", quietCostRatio);
        std::printf ("\n");
    }
    std::printf ("\n%s\n", overBudget ? "Over the CPU budget" : "Within the CPU budget");
    return overBudget ? 1 : 0;
}
