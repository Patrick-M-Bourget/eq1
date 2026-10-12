// Measures the Engine's CPU load in each CPU scene and fails when a scene with a ceiling is over budget
// (docs/performance.md, "CPU budget"). Run by scripts/check.sh cpu, on its own: timings taken while
// other tests run are meaningless.
//
// It can also run one scene at a time, for comparing two builds:
//   eq1_cpu_budget scenes                          the CPU scenes' names, one per line, in order
//   eq1_cpu_budget run <scene> <rate> <seconds>    the load of scene number <scene> (from 0) at <rate> Hz,
//                                                  timed for about <seconds> of wall-clock time, as one number

#include "../engine/Measure.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#if ! defined(_WIN32)
    #include <time.h>
#endif

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

// Changes the settings for the block starting at the given time in seconds.
using Automation = void (*) (Settings&, double seconds);

// Band 1's Gain swept by Host Automation, 3 dB either side of where it was, once every 2 s: every block
// gets settings that differ from the last block's.
void automateGain (Settings& settings, double seconds)
{
    settings.bands[0].gain = 3.0 + 3.0 * std::sin (std::numbers::pi * seconds);
}

// A ceiling, in percent of real time.
struct Ceiling
{
    double at48kHz, at96kHz;
};

// A CPU scene: what the Engine runs, on what, timed from startSeconds in. With automate, the settings
// are published before every block, as the plugin publishes them while Host Automation moves a
// parameter, and the publication is timed with the block.
struct Scene
{
    const char* name;
    Settings settings;
    int blockSize;
    Signal signal;
    double startSeconds = 0.0;
    Automation automate = nullptr;
    std::optional<Ceiling> ceiling = {};
    bool quiet = false; // held to quietCostRatio times the first scene's load
};

// The CPU scenes, in the order printed. The first one's load is the quiet-cost rule's reference.
std::vector<Scene> scenes()
{
    return {
        { "24 Dynamic Bells", dynamicBells(), 512, bursts, .ceiling = Ceiling { 35.0, 60.0 } },
        { "24 Bells", test::bells(), 512, bursts },
        { "1 Bell", test::bell (1000.0, 3.0, 1.0), 512, bursts },
        { "24 Brickwall Cuts", test::brickwallCuts(), 512, bursts },
        { "24 Bells, Output Gain and Pan", withOutputGainAndPan (test::bells()), 512, bursts },
        { "24 Dynamic Bells, 32-sample blocks", dynamicBells(), 32, bursts },
        { "24 Dynamic Bells, Gain automated", dynamicBells(), 512, bursts, .automate = automateGain,
          .ceiling = Ceiling { 35.0, 60.0 } },
        // Timed after 10 s of silence, long enough for every filter to have rung down below the
        // smallest normal float.
        { "24 Dynamic Bells on silence", dynamicBells(), 512, thenSilence, 10.0, .quiet = true },
        { "24 Dynamic Bells on subnormal input", dynamicBells(), 512, subnormalInput, .quiet = true },
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
        auto settings = scene.settings;
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
            if (scene.automate != nullptr)
            {
                scene.automate (settings, static_cast<double> (block * scene.blockSize) / sampleRate);
                engine.setSettings (settings);
            }
            engine.process ({ main, 2, scene.blockSize });
            if (block >= firstTimed)
                busy += std::chrono::steady_clock::now() - start;
        }
        loads.push_back (100.0 * std::chrono::duration<double> (busy).count() / secondsPerRun);
    }
    std::sort (loads.begin(), loads.end());
    return loads[loads.size() / 2];
}

// The processing time this thread has used, in seconds: CPU time where the system gives it per thread,
// so time the thread spends waiting for a core on a busy machine isn't counted; wall-clock time otherwise.
double threadSeconds()
{
#if defined(_WIN32)
    return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
#else
    timespec now {};
    clock_gettime (CLOCK_THREAD_CPUTIME_ID, &now);
    return static_cast<double> (now.tv_sec) + 1.0e-9 * static_cast<double> (now.tv_nsec);
#endif
}

// One A/B run: the Engine's load in a scene, in percent of real time, in a fresh Engine, timed for about
// wallSeconds of wall-clock time after the scene's start. The input is made a tenth of a second at a
// time ahead of processing it, and only the processing is timed, by the thread's CPU time.
double abLoadPercent (const Scene& scene, double sampleRate, double wallSeconds)
{
    test::Noise noise (1);
    const auto block = static_cast<size_t> (scene.blockSize);
    const auto chunkBlocks = std::max<size_t> (1, static_cast<size_t> (0.1 * sampleRate) / block);
    std::vector<float> left (chunkBlocks * block), right (chunkBlocks * block);
    Engine engine;
    engine.prepare (sampleRate, scene.blockSize, 2);
    engine.setSettings (scene.settings);
    auto settings = scene.settings;
    const auto firstTimed = static_cast<long> (scene.startSeconds * sampleRate) / scene.blockSize;
    long next = 0; // the next block's number
    double busy = 0.0;
    long timedSamples = 0;
    std::optional<std::chrono::steady_clock::time_point> timingSince;
    while (! timingSince || std::chrono::duration<double> (std::chrono::steady_clock::now() - *timingSince).count() < wallSeconds)
    {
        for (size_t i = 0; i < left.size(); ++i)
        {
            const double seconds = static_cast<double> (next * scene.blockSize + static_cast<long> (i)) / sampleRate;
            left[i] = scene.signal (seconds, noise());
            right[i] = scene.signal (seconds, noise());
        }
        const bool timed = next >= firstTimed;
        if (timed && ! timingSince)
            timingSince = std::chrono::steady_clock::now();
        const double start = threadSeconds();
        for (size_t b = 0; b < chunkBlocks; ++b, ++next)
        {
            if (scene.automate != nullptr)
            {
                scene.automate (settings, static_cast<double> (next * scene.blockSize) / sampleRate);
                engine.setSettings (settings);
            }
            float* main[] = { left.data() + b * block, right.data() + b * block };
            engine.process ({ main, 2, scene.blockSize });
        }
        if (timed)
        {
            busy += threadSeconds() - start;
            timedSamples += static_cast<long> (left.size());
        }
    }
    return 100.0 * busy * sampleRate / static_cast<double> (timedSamples);
}

// The sample rates the budget is measured at.
constexpr double sampleRates[] = { 48000.0, 96000.0 };

// Silence and subnormal input may cost no more than this times the 24 Dynamic Bells' music. Subnormal
// arithmetic is many times slower on x64, so a filter left to ring down into subnormal numbers is far over it.
constexpr double quietCostRatio = 1.5;

int budget()
{
    bool overBudget = false;
    std::printf ("Engine CPU load, stereo, in percent of real time: the median of %d runs of %.0f s, in 512-sample blocks unless named\n\n",
                 runs, secondsPerRun);
    const auto all = scenes();
    for (const double sampleRate : sampleRates)
    {
        double music = 0.0;
        for (const auto& scene : all)
        {
            const double load = loadPercent (scene, sampleRate);
            if (&scene == &all.front())
                music = load;
            std::printf ("%-36s %6.1f kHz  %6.2f%%", scene.name, sampleRate / 1000.0, load);
            if (scene.ceiling)
            {
                const double ceiling = sampleRate < 64000.0 ? scene.ceiling->at48kHz : scene.ceiling->at96kHz;
                const bool over = load > ceiling;
                overBudget = overBudget || over;
                std::printf ("  (ceiling %4.1f%%)%s", ceiling, over ? "  OVER" : "");
            }
            if (scene.quiet && load > quietCostRatio * music)
            {
                overBudget = true;
                std::printf ("  OVER: more than %.1f x the 24 Dynamic Bells", quietCostRatio);
            }
            std::printf ("\n");
        }
        std::printf ("\n");
    }
    std::printf ("%s\n", overBudget ? "Over the CPU budget" : "Within the CPU budget");
    return overBudget ? 1 : 0;
}

int usage()
{
    std::fprintf (stderr, "usage: eq1_cpu_budget [scenes | run <scene> <sample rate> <seconds>]\n");
    return 2;
}

} // namespace

int main (int argc, char** argv)
{
    if (argc == 1)
        return budget();
    const std::string command = argv[1];
    const auto all = scenes();
    if (command == "scenes" && argc == 2)
    {
        for (const auto& scene : all)
            std::printf ("%s\n", scene.name);
        return 0;
    }
    if (command == "run" && argc == 5)
    {
        const int scene = std::atoi (argv[2]);
        const double sampleRate = std::atof (argv[3]), seconds = std::atof (argv[4]);
        if (scene < 0 || scene >= static_cast<int> (all.size()) || sampleRate <= 0.0 || seconds <= 0.0)
            return usage();
        std::printf ("%.6f\n", abLoadPercent (all[static_cast<size_t> (scene)], sampleRate, seconds));
        return 0;
    }
    return usage();
}
