#pragma once

#include "Biquad.h"
#include "Smoother.h"
#include "eq1/Settings.h"

#include <vector>

namespace eq1
{

// The live processing of one Band slot. Frequency, Q and Gain glide to new settings, and the Band
// crossfades in and out as it is put in use, taken out of use, Bypassed or un-Bypassed.
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
    void designCoefficients();
    bool isSilent() const { return mix.value() == 0.0 && ! mix.isMoving(); }

    double sampleRate = 44100.0;
    Smoother logFrequency, gain, logQ, mix;
    BiquadCoefficients coefficients;
    std::vector<BiquadState> states; // per channel
};

} // namespace eq1
