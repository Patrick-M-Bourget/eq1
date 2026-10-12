#pragma once

#include "Smoother.h"

namespace eq1
{

// A crossfade over a piece of a run, in place: each sample moves from what its channel holds towards
// to (ch, i) by the glide's amount, 0 leaving it and 1 reaching to. The amount is the glide's next
// value a sample while it moves, else its value, taken once a sample for every channel. The Bands
// mix their wet signal with the dry one and the previous chain's with the current one this way; the
// Engine mixes the held output (Solo, Detection Audition) and Global Bypass in.
//
// The difference to - from is taken in Difference, and the rest in double. A float difference rounds,
// so a full mix, from + 1.0 * (to - from), can miss to by an ulp: where a full mix must play to
// exactly, as the held output does, the difference is in double. The Bands and Global Bypass take it
// in float, which their output is defined by: double would move their samples by an ulp.
template <typename Difference, typename To>
void crossfade (Smoother& glide, float* const* channels, int numChannels, int numSamples, To to)
{
    const bool moving = glide.isMoving();
    for (int i = 0; i < numSamples; ++i)
    {
        const double amount = moving ? glide.next() : glide.value();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float& sample = channels[ch][i];
            const Difference difference = static_cast<Difference> (to (ch, i)) - static_cast<Difference> (sample);
            sample = static_cast<float> (sample + amount * difference);
        }
    }
}

} // namespace eq1
