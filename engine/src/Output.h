#pragma once

#include "Smoother.h"
#include "eq1/Settings.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace eq1
{

// The output stage after every Band: Output Gain, Output Pan in its Pan Mode and Phase Invert, folded
// into one mix of the two channels, each of whose four amounts glides, so any change is click-free.
class Output
{
public:
    void prepare (double sampleRate, int channels)
    {
        numChannels = channels;
        for (auto& amount : amounts)
            amount.configure (glideTimeConstantSeconds * sampleRate, 1.0e-6);
    }

    // With snap, the new settings take effect at once instead of gliding. extraGainDb is added to
    // Output Gain.
    void setSettings (const Settings& settings, double extraGainDb, bool snap)
    {
        const auto target = mixOf (settings, extraGainDb);
        for (size_t i = 0; i < amounts.size(); ++i)
        {
            if (snap)
                amounts[i].reset (target[i]);
            else
                amounts[i].setTarget (target[i]);
        }
    }

    void process (float* const* channels, int channelCount, int numSamples)
    {
        if (channelCount < 2 || numChannels < 2)
        {
            processMono (channels, channelCount, numSamples);
            return;
        }
        if (! moving() && isIdentity())
            return;
        float* left = channels[0];
        float* right = channels[1];
        for (int i = 0; i < numSamples; ++i)
        {
            const double ll = amounts[0].next(), lr = amounts[1].next(), rl = amounts[2].next(), rr = amounts[3].next();
            const double l = left[i], r = right[i];
            left[i] = static_cast<float> (ll * l + lr * r);
            right[i] = static_cast<float> (rl * l + rr * r);
        }
    }

private:
    static constexpr double glideTimeConstantSeconds = 0.007; // as a Band's glides: about 50 ms

    // left = ll * left + lr * right, right = rl * left + rr * right; on mono only ll applies.
    using Mix = std::array<double, 4>;

    Mix mixOf (const Settings& settings, double extraGainDb) const
    {
        const double gain = std::pow (10.0, (settings.outputGainDb + extraGainDb) / 20.0) * (settings.phaseInvert ? -1.0 : 1.0);
        if (numChannels < 2)
            return { gain, 0.0, 0.0, 0.0 };
        const double pan = std::clamp (settings.outputPan, -1.0, 1.0);
        const double first = gain * std::min (1.0, 1.0 - pan), second = gain * std::min (1.0, 1.0 + pan);
        if (settings.panMode == PanMode::LeftRight)
            return { first, 0.0, 0.0, second };
        // Encode Mid and Side, scale them, decode: L = M + S, R = M - S.
        const double same = 0.5 * (first + second), cross = 0.5 * (first - second);
        return { same, cross, cross, same };
    }

    void processMono (float* const* channels, int channelCount, int numSamples)
    {
        if (! moving() && amounts[0].value() == 1.0)
            return;
        for (int i = 0; i < numSamples; ++i)
        {
            for (size_t a = 1; a < amounts.size(); ++a)
                amounts[a].next();
            const double gain = amounts[0].next();
            for (int ch = 0; ch < channelCount; ++ch)
                channels[ch][i] = static_cast<float> (gain * channels[ch][i]);
        }
    }

    bool moving() const
    {
        return std::any_of (amounts.begin(), amounts.end(), [] (const Smoother& amount) { return amount.isMoving(); });
    }

    bool isIdentity() const
    {
        return amounts[0].value() == 1.0 && amounts[1].value() == 0.0 && amounts[2].value() == 0.0 && amounts[3].value() == 1.0;
    }

    int numChannels = 0;
    std::array<Smoother, 4> amounts;
};

} // namespace eq1
