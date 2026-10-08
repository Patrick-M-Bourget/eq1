#pragma once

#include "ShapeDesign.h"
#include "Smoother.h"
#include "eq1/Settings.h"

#include <array>
#include <optional>
#include <vector>

namespace eq1
{

// The live processing of one Band slot. Frequency, Q and Gain glide to new settings. The Band
// crossfades in and out as it is put in use, taken out of use, Bypassed or un-Bypassed, and from
// its old filter to its new one when its Shape, Slope or Stereo Placement changes.
class Band
{
public:
    // The longest run of samples process() may be given; coefficients follow glides at this rate.
    static constexpr int maxSubBlock = 16;

    void prepare (double sampleRate, int numChannels);

    // With snap, the new settings take effect at once instead of gliding.
    void setSettings (const BandSettings& settings, bool snap);

    void process (float* const* channels, int numChannels, int numSamples);

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
        // Runs samples through the cascade. With from, the coefficients move from it to the
        // cascade's over the run.
        void run (size_t channel, float* samples, int numSamples, const Cascade* from);
        // Runs the part of the signal the chain's Stereo Placement selects, in place.
        void runPlaced (float* const* channels, int numChannels, int numSamples, const Cascade* from);
        // On stereo: encodes Mid and Side, runs the Mid (mid) or the Side, and decodes.
        void runMidSide (float* const* channels, int numSamples, const Cascade* from, bool mid);
    };

    void design();
    void crossfadeTo (const Setup& setup);
    bool isSilent() const { return mix.value() == 0.0 && ! mix.isMoving(); }

    double sampleRate = 44100.0;
    Smoother logFrequency, gain, logQ;
    Smoother mix;   // 0 = no effect, 1 = full effect
    Smoother shapeCrossfade; // 0 = previous chain, 1 = current chain
    Chain current, previous;

    // Per channel, for mixing the filtered signal with the dry one, and the previous chain's with the current.
    std::vector<std::array<float, maxSubBlock>> wet, previousWet;
    std::vector<float*> wetChannels, previousWetChannels;

    // A Shape, Slope or Stereo Placement change that arrived during a crossfade waits for it to
    // finish, since only two filters can play at once.
    std::optional<Setup> pending;
};

} // namespace eq1
