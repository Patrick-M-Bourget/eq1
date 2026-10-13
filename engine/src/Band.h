#pragma once

#include "RunGrid.h"
#include "ShapeDesign.h"
#include "Smoother.h"
#include "eq1/Settings.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace eq1
{

// The live processing of one Band slot. Frequency, Q and Gain glide to new settings. The Band
// crossfades in and out as it is put in use, taken out of use, Bypassed or un-Bypassed, and from
// its old filter to its new one when its Shape, Slope or Stereo Placement changes.
//
// It runs on the Engine's grid of runs of runSamples samples: its filter is re-designed once at the
// start of each run, and its coefficients move to the new design over the run by position in it, so
// a run cut into pieces sounds the same as a whole one.
class Band
{
public:
    void prepare (double sampleRate, int numChannels);

    // With snap, the new settings take effect at once instead of gliding.
    void setSettings (const BandSettings& settings, bool snap);

    // Moves the Band's Live Gain away from its Gain by db, from the next run's start: its dynamics.
    void setDynamicOffset (double db) { dynamicOffset = db; }

    // About the Gain the Band's filter applies at the last sample processed (Gain plus the dynamic
    // offset, held to +/-30 dB), for the display: linear in dB between the run's start and end, which
    // the coefficients only approximate.
    double liveGainDb() const;

    // Processes the piece of the grid's run numbered run (counted by the Engine from prepare) from
    // position (0 to runSamples - 1) on, numSamples long, at most to the run's end. A run's first
    // piece starts at position 0. A Band that missed the run's start (a detector with nothing to
    // listen to for a while) plays at the filter last designed until the next run; one that saw the
    // start and misses a piece in the middle keeps gliding by position.
    void process (float* const* channels, int numChannels, std::uint64_t run, int position, int numSamples);

    // How many sections' passes over a piece of a run the Band has skipped since prepare(), each one
    // exactly the identity with nothing in its state (Chain::run). Safe to call from any thread, for tests.
    std::uint64_t sectionsSkipped() const { return skipped.load (std::memory_order_relaxed); }

private:
    // What a chain is: its filter's structure and the part of the signal it plays on. A Band crossfades
    // from one to another.
    struct Setup
    {
        Structure structure;
        StereoPlacement placement = StereoPlacement::Stereo;

        bool operator== (const Setup&) const = default;
    };

    // A cascade with its state for each channel.
    struct Chain
    {
        Setup setup;
        Cascade cascade;
        std::vector<std::array<BiquadState, maxSections>> states;

        void clear();
        // Clears every section whose state isn't finite, and says whether any was: a NaN or infinite
        // input sample would otherwise stay in the state, and so in the output, until the next prepare.
        bool clearNonFinite();
        // The samples of a run from position on, and how the coefficients move over the run: with from,
        // from it to the cascade's, reaching it at the run's end.
        struct Piece
        {
            int position = 0;
            int numSamples = 0;
            const Cascade* from = nullptr;

            // How far the coefficients have moved from from to the cascade's at the piece's sample i:
            // the fraction of the run done once it is processed, so the run's last sample reaches 1.
            double reached (size_t i) const { return (position + static_cast<double> (i) + 1.0) / runSamples; }
        };

        // The channels a cascade runs on together: each one's samples, and which channel's section
        // states it keeps. The chain's Stereo Placement fills it.
        struct Lines
        {
            std::vector<float*> samples;
            std::vector<size_t> states;
            int count = 0;

            void prepare (int numChannels);
        };

        // Whether section s is skipped over the piece: exactly the identity at both ends of the run, so
        // at every position in it, with every line's state clear. Its samples and state stay as they
        // are, which is what running it leaves where nothing fuses; on arm64 running it would add a
        // fused multiply-add's rounding residual, near -300 dBFS (docs/performance.md, "Identity
        // sections"). One with state left from filtering runs until that has drained. The section
        // count stays fixed for glides.
        bool skips (size_t s, const Lines& lines, const Piece& piece) const;
        // Runs the lines' samples through the cascade, in place: over a gliding run, with the
        // coefficients interpolated once per position, for every line at once. Two lines (a Stereo
        // Band on a stereo track) run in two lanes. Returns how many sections it skipped.
        int run (Lines& lines, const Piece& piece);
        // Two lines through the cascade together, one per lane of a Double2: the same operations in
        // the same order as one line on its own, so each comes out bit for bit as it would alone.
        int runLanes (Lines& lines, const Piece& piece);
        // Runs the part of the signal the chain's Stereo Placement selects, in place.
        int runPlaced (float* const* channels, int numChannels, Lines& lines, const Piece& piece);
        // On stereo: encodes Mid and Side, runs the Mid (mid) or the Side, and decodes.
        int runMidSide (float* const* channels, Lines& lines, const Piece& piece, bool mid);
    };

    // Once per run, at its start: a waiting crossfade starts, and a glide moves on by the whole run
    // and re-designs the filter.
    void startRun();
    double targetGainDb() const;
    void design();
    // Designs the filter at once, to play unchanged until the next run.
    void designNow();
    void crossfadeTo (const Setup& setup);
    bool isSilent() const { return mix.value() == 0.0 && ! mix.isMoving(); }
    void countSkipped (int sections)
    {
        if (sections > 0)
            skipped.fetch_add (static_cast<std::uint64_t> (sections), std::memory_order_relaxed);
    }

    double sampleRate = 44100.0;
    Smoother logFrequency, gain, logQ;
    Smoother mix;   // 0 = no effect, 1 = full effect
    Smoother shapeCrossfade; // 0 = previous chain, 1 = current chain
    Chain current, previous;
    Chain::Lines lines; // sized at prepare(), for either chain's run
    double dynamicOffset = 0.0, designedOffset = 0.0; // dB
    std::atomic<std::uint64_t> skipped { 0 };         // sections skipped, for sectionsSkipped()

    // The run in progress: the last one whose start the Band saw; whether the coefficients move over
    // it, and from what; where the last piece ended; and the Live Gain at its start and its end, for
    // liveGainDb().
    static constexpr std::uint64_t noRun = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t startedRun = noRun;
    bool runGliding = false;
    Cascade runFrom;
    int runPosition = 0;
    double runStartGainDb = 0.0, runEndGainDb = 0.0;

    // Per channel, for mixing the filtered signal with the dry one, and the previous chain's with the current.
    std::vector<std::array<float, runSamples>> wet, previousWet;
    std::vector<float*> wetChannels, previousWetChannels;

    // A Shape, Slope or Stereo Placement change that arrived during a crossfade waits for it to
    // finish, since only two filters can play at once.
    std::optional<Setup> pending;
};

} // namespace eq1
