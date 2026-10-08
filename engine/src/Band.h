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
// its old filter to its new one when its Shape or Slope changes.
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
    // A cascade with its state for each channel.
    struct Chain
    {
        Structure structure;
        Cascade cascade;
        std::vector<std::array<BiquadState, maxSections>> states;

        void clear();
        // Runs samples through the cascade. With from, the coefficients move from it to the
        // cascade's over the run.
        void run (size_t channel, float* samples, int numSamples, const Cascade* from);
    };

    void design();
    void crossfadeTo (const Structure& structure);
    bool isSilent() const { return mix.value() == 0.0 && ! mix.isMoving(); }

    double sampleRate = 44100.0;
    Smoother logFrequency, gain, logQ;
    Smoother mix;   // 0 = no effect, 1 = full effect
    Smoother shapeCrossfade; // 0 = previous chain, 1 = current chain
    Chain current, previous;

    // A Shape or Slope change that arrived during a crossfade waits for it to finish, since only
    // two filters can play at once.
    std::optional<Structure> pending;
};

} // namespace eq1
