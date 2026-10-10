#pragma once

#include "ShapeDesign.h"
#include "Smoother.h"
#include "eq1/Settings.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace eq1
{

// The live processing of one Band slot. Frequency, Q and Gain glide to new settings. The Band
// crossfades in and out as it is put in use, taken out of use, Bypassed or un-Bypassed, and from
// its old filter to its new one when its Shape, Slope or Stereo Placement changes.
//
// It runs on the Engine's grid of runs of maxSubBlock samples: its filter is re-designed once at the
// start of each run, and its coefficients move to the new design over the run by position in it, so
// a run cut into pieces sounds the same as a whole one.
class Band
{
public:
    // The grid's run length; coefficients follow glides at this rate.
    static constexpr int maxSubBlock = 16;
    static constexpr std::uint64_t noRun = UINT64_MAX;

    void prepare (double sampleRate, int numChannels);

    // With snap, the new settings take effect at once instead of gliding.
    void setSettings (const BandSettings& settings, bool snap);

    // Moves the Band's Live Gain away from its Gain by db, from the next run's start: its dynamics.
    void setDynamicOffset (double db) { dynamicOffset = db; }

    // The Gain the Band's filter applies at the last sample processed: Gain plus the dynamic offset,
    // held to +/-30 dB.
    double liveGainDb() const;

    // Processes the piece of the grid's run number run from position (0 to maxSubBlock - 1) on,
    // numSamples long, at most to the run's end. A run's first piece starts at position 0. A Band that
    // missed the run's start (a detector with nothing to listen to for a while) plays at the filter
    // last designed until the next run.
    void process (float* const* channels, int numChannels, std::uint64_t run, int position, int numSamples);

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
        // The samples of a run from position on, and how the coefficients move over the run: with from,
        // from it to the cascade's, reaching it at the run's end.
        struct Piece
        {
            int position = 0;
            int numSamples = 0;
            const Cascade* from = nullptr;
        };

        // Runs samples through the cascade.
        void run (size_t channel, float* samples, const Piece& piece);
        // Runs the part of the signal the chain's Stereo Placement selects, in place.
        void runPlaced (float* const* channels, int numChannels, const Piece& piece);
        // On stereo: encodes Mid and Side, runs the Mid (mid) or the Side, and decodes.
        void runMidSide (float* const* channels, const Piece& piece, bool mid);
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

    double sampleRate = 44100.0;
    Smoother logFrequency, gain, logQ;
    Smoother mix;   // 0 = no effect, 1 = full effect
    Smoother shapeCrossfade; // 0 = previous chain, 1 = current chain
    Chain current, previous;
    double dynamicOffset = 0.0, designedOffset = 0.0; // dB

    // The run in progress: the last one whose start the Band saw; whether the coefficients move over
    // it, and from what; where the last piece ended; and the Live Gain at its start and its end, for
    // liveGainDb().
    std::uint64_t startedRun = noRun;
    bool runGliding = false;
    Cascade runFrom;
    int runPosition = 0;
    double runStartGainDb = 0.0, runEndGainDb = 0.0;

    // Per channel, for mixing the filtered signal with the dry one, and the previous chain's with the current.
    std::vector<std::array<float, maxSubBlock>> wet, previousWet;
    std::vector<float*> wetChannels, previousWetChannels;

    // A Shape, Slope or Stereo Placement change that arrived during a crossfade waits for it to
    // finish, since only two filters can play at once.
    std::optional<Setup> pending;
};

} // namespace eq1
