#pragma once

#include "eq1/Settings.h"

#include <cstdint>
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

// What the Output Level and the Detection Level read for silence, and for anything quieter.
inline constexpr double levelFloorDb = -150.0;

// The Output Level of one channel, in dBFS.
struct OutputLevel
{
    double peakDb = levelFloorDb; // sample peak since the last read
    double rmsDb = levelFloorDb;  // over the last 300 ms
};

// The DSP Engine. prepare() may allocate; setSettings(), process(), readAnalysis(), skipAnalysis(), liveGainDb(),
// outputLevelChannels(), readOutputLevel() and readDetectionLevel() never allocate, lock or do I/O. process() flushes
// subnormal numbers to zero, whatever the caller's floating-point mode, and leaves that mode as it
// found it.
//
// Threads: process() runs on the audio thread. setSettings() may run on another thread, but only
// one thread at a time may call it; the newest settings are taken at the start of each process().
// readAnalysis() and skipAnalysis() may run on one reader thread, and readOutputLevel() and readDetectionLevel() each on
// one reader thread, through any prepare(): a re-prepare at another sample rate or channel count
// never frees or moves what a reader touches.
class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void setSettings (const Settings& settings);

    // Processes main in place. sidechain may be null when nothing is connected. Blocks may be of any
    // size, changing from call to call. The output doesn't depend on how the host cuts the audio into
    // blocks, sample for sample, Dynamic Bands included: they move on a grid of 16-sample runs that
    // starts at prepare() and carries across calls, and a Dynamic Band's gain over a run follows what
    // its detector heard up to the run's start, so it reacts up to one run later, with no latency.
    void process (AudioBlock main, const ConstAudioBlock* sidechain = nullptr);

    // Copies up to maxSamples of the tap's mono signal, oldest first, and returns how many were copied.
    // Called from one reader thread while process() runs on the audio thread.
    int readAnalysis (AnalysisTap tap, float* destination, int maxSamples);

    // Drops what the tap holds, so the next readAnalysis() copies only what process() pushed after
    // this: a reader starting afresh, the editor opening, sees current audio rather than what the tap
    // kept while nothing read it. From readAnalysis()'s reader thread.
    void skipAnalysis (AnalysisTap tap);

    // How many samples a tap holds between reads: the reader's buffer is this big. Safe to call from
    // any thread.
    int analysisCapacity() const;

    // The Live Gain in dB a Band Slot (1 to 24) applied at the end of the last process(): its Gain,
    // moved by its dynamics, held to +/-30 dB. Safe to call from any thread, for the display.
    double liveGainDb (int slot) const;

    // How many channels the Output Level has: one per channel the Engine was prepared with.
    int outputLevelChannels() const;

    // The Output Level of a channel (0 to outputLevelChannels() - 1): what process() left there, after
    // everything, so the input during Global Bypass. Reading resets the peak, so a peak between two
    // reads is reported once, by the next. Called from one reader thread while process() runs on the
    // audio thread.
    OutputLevel readOutputLevel (int channel);

    // The Detection Level of the metered Band Slot (Settings::meteredSlot), in dB on Threshold's scale:
    // the loudest level its detector compared with Threshold since the last read, the louder channel's
    // when it hears two. Reading resets it. The floor when no Band is metered, or the metered Band has
    // nothing to listen to. Called from one reader thread while process() runs on the audio thread.
    double readDetectionLevel();

    // How many times process() has applied settings: once at the first block after prepare(), then
    // once per snapshot that differs from the settings it plays. A snapshot equal to them is skipped,
    // so a block with no edits does no settings work. Safe to call from any thread, for tests.
    std::uint64_t settingsApplied() const;

    // How many sections' passes over a piece of a run the Bands have skipped since prepare(), each one
    // exactly the identity (an unsplit resonant shelf's second biquad) with nothing in its state
    // (docs/performance.md, "Identity sections"). Safe to call from any thread, for tests.
    std::uint64_t sectionsSkipped() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace eq1
