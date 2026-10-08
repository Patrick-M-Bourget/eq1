#pragma once

#include "eq1/Settings.h"

#include <memory>

namespace eq1
{

struct AudioBlock
{
    float* const* channels = nullptr;
    int numChannels = 0;
    int numSamples = 0;
};

struct ConstAudioBlock
{
    const float* const* channels = nullptr;
    int numChannels = 0;
    int numSamples = 0;
};

// Where the Analyzer listens: the main input before and after the EQ, and the Sidechain.
enum class AnalysisTap
{
    PreEq,
    PostEq,
    Sidechain,
};

// The DSP Engine. prepare() may allocate; setSettings(), process(), readAnalysis() and
// liveGainDb() never allocate, lock or do I/O.
//
// Threads: process() runs on the audio thread. setSettings() may run on another thread, but only
// one thread at a time may call it; the newest settings are taken at the start of each process().
// readAnalysis() may run on one reader thread.
class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void setSettings (const Settings& settings);

    // Processes main in place. sidechain may be null when nothing is connected.
    void process (AudioBlock main, const ConstAudioBlock* sidechain = nullptr);

    // Copies up to maxSamples of the tap's mono signal, oldest first, and returns how many were copied.
    // Called from one reader thread while process() runs on the audio thread.
    int readAnalysis (AnalysisTap tap, float* destination, int maxSamples);

    // The Live Gain in dB a Band Slot (1 to 24) applied at the end of the last process(): its Gain,
    // moved by its dynamics, held to +/-30 dB. Safe to call from any thread, for the display.
    double liveGainDb (int slot) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace eq1
