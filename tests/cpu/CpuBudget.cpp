// Measures the Engine's CPU load in each CPU scene and fails when the 24 Dynamic Bells scene is over
// budget (docs/performance.md, "CPU budget"). Run by scripts/check.sh cpu, on its own: timings taken
// while other tests run are meaningless.

#include "../engine/Measure.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace eq1;

namespace
{

constexpr int runs = 5;
constexpr double secondsPerRun = 4.0;

// The 24 Dynamic Bells (tests/engine/Measure.h), one of them metered.
Settings dynamicBells()
{
    auto settings = test::dynamicBells();
    settings.meteredSlot = 1;
    return settings;
}

// 24 Brickwall Cuts across the spectrum: Low Cuts in the even slots, High Cuts in the odd ones.
Settings brickwallCuts()
{
    return test::spreadBands ([] (size_t slot) {
        return BandSettings { .shape = slot % 2 == 0 ? Shape::LowCut : Shape::HighCut, .brickwall = true };
    });
}

Settings withOutputGainAndPan (Settings settings)
{
    settings.outputGainDb = 3.0;
    settings.outputPan = 0.3;
    return settings;
}

// What the input carries, as a function of the sample's time in seconds and a noise sample.
using Signal = std::function<float (double seconds, float noise)>;

// Noise that is loud for 100 ms of every 200, so the Bands keep moving.
float bursts (double seconds, float noise) { return std::fmod (seconds, 0.2) < 0.1 ? 0.5f * noise : 0.01f * noise; }

// Half a second of noise, then nothing.
float thenSilence (double seconds, float noise) { return seconds < 0.5 ? noise : 0.0f; }

// Input that is all subnormal numbers.
float subnormalInput (double, float noise) { return 1.0e-39f * noise; }

// A CPU scene: what the Engine runs, on what, timed from startSeconds in.
struct Scene
{
    const char* name;
    Settings settings;
    int blockSize;
    Signal signal;
    double startSeconds = 0.0;
};

// The CPU scenes, in the order printed. The first is the one with a ceiling; the last, the 24 Dynamic
// Bells on silence, is one side of the quiet-cost rule (the other, subnormal input, isn't a scene of
// its own and is measured after them).
std::vector<Scene> scenes()
{
    return {
        { "24 Dynamic Bells", dynamicBells(), 512, bursts },
        { "24 Bells", test::bells(), 512, bursts },
        { "1 Bell", test::bell (1000.0, 3.0, 1.0), 512, bursts },
        { "24 Brickwall Cuts", brickwallCuts(), 512, bursts },
        { "24 Bells, Output Gain and Pan", withOutputGainAndPan (test::bells()), 512, bursts },
        { "24 Dynamic Bells, 32-sample blocks", dynamicBells(), 32, bursts },
        // Timed after 10 s of silence, long enough for every filter to have rung down below the
        // smallest normal float.
        { "24 Dynamic Bells on silence", dynamicBells(), 512, thenSilence, 10.0 },
    };
}

// The Engine's load in a scene, in percent of real time: the median of several runs, each in a fresh Engine.
double loadPercent (const Scene& scene, double sampleRate)
{
    std::vector<double> loads;
    test::Noise noise (1);
    std::vector<float> left (static_cast<size_t> (scene.blockSize)), right (static_cast<size_t> (scene.blockSize));
    float* main[] = { left.data(), right.data() };
    for (int run = 0; run < runs; ++run)
    {
        Engine engine;
        engine.prepare (sampleRate, scene.blockSize, 2);
        engine.setSettings (scene.settings);
        const auto blocks = static_cast<long> ((scene.startSeconds + secondsPerRun) * sampleRate / scene.blockSize);
        const auto firstTimed = static_cast<long> (scene.startSeconds * sampleRate / scene.blockSize);
        std::chrono::steady_clock::duration busy {};
        for (long block = 0; block < blocks; ++block)
        {
            for (size_t i = 0; i < left.size(); ++i)
            {
                const double seconds = static_cast<double> (block * scene.blockSize + static_cast<long> (i)) / sampleRate;
                left[i] = scene.signal (seconds, noise());
                right[i] = scene.signal (seconds, noise());
            }
            const auto start = std::chrono::steady_clock::now();
            engine.process ({ main, 2, scene.blockSize });
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
    double ceilingPercent; // for the 24 Dynamic Bells scene; the other scenes have none yet
};

// docs/performance.md, "CPU budget".
constexpr Budget budgets[] = { { 48000.0, 35.0 }, { 96000.0, 60.0 } };

// Silence and subnormal input may cost no more than this times the 24 Dynamic Bells' music. Subnormal
// arithmetic is many times slower on x64, so a filter left to ring down into subnormal numbers is far over it.
constexpr double quietCostRatio = 1.5;

} // namespace

int main()
{
    bool overBudget = false;
    std::printf ("Engine CPU load, stereo, in percent of real time: the median of %d runs of %.0f s, in 512-sample blocks unless named\n\n",
                 runs, secondsPerRun);
    const auto all = scenes();
    for (const auto& [sampleRate, ceiling] : budgets)
    {
        double music = 0.0;
        for (const auto& scene : all)
        {
            const double load = loadPercent (scene, sampleRate);
            std::printf ("%-36s %6.1f kHz  %6.2f%%", scene.name, sampleRate / 1000.0, load);
            if (&scene == &all.front())
            {
                music = load;
                const bool over = load > ceiling;
                overBudget = overBudget || over;
                std::printf ("  (ceiling %4.1f%%)%s", ceiling, over ? "  OVER" : "");
            }
            else if (&scene == &all.back() && load > quietCostRatio * music)
            {
                overBudget = true;
                std::printf ("  OVER: more than %.1f x the 24 Dynamic Bells", quietCostRatio);
            }
            std::printf ("\n");
        }
        const double subnormal = loadPercent ({ "", dynamicBells(), 512, subnormalInput }, sampleRate);
        const bool over = subnormal > quietCostRatio * music;
        overBudget = overBudget || over;
        std::printf ("%-36s %6.1f kHz  %6.2f%%", "24 Dynamic Bells on subnormal input", sampleRate / 1000.0, subnormal);
        if (over)
            std::printf ("  OVER: more than %.1f x the 24 Dynamic Bells", quietCostRatio);
        std::printf ("\n\n");
    }
    std::printf ("%s\n", overBudget ? "Over the CPU budget" : "Within the CPU budget");
    return overBudget ? 1 : 0;
}
