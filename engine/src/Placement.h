#pragma once

#include "eq1/Settings.h"

namespace eq1
{

// Stereo Placement routing: which part of the signal a Placement selects, how Mid and Side are
// encoded from Left and Right and decoded back, and where a part plays back. Every site that
// selects, encodes or decodes a Placement (a Band's chains, a detector's detection signal, Solo's
// region, the held output) comes here, so they can't drift apart.

// Mid and Side from Left and Right, and back, in float: a Band's Mid or Side chain runs on float
// samples, and a detector or Solo encodes the same way, so they hear what the Band filters.
inline float encodeMid (float left, float right) { return 0.5f * (left + right); }
inline float encodeSide (float left, float right) { return 0.5f * (left - right); }
inline float decodeLeft (float mid, float side) { return mid + side; }
inline float decodeRight (float mid, float side) { return mid - side; }

// Routes a signal of numChannels channels by placement, and returns how many channels the part it
// selects has. channel (part, ch) is called for each channel ch the Placement takes as it is, numbered
// part from 0 within the part: every channel for Stereo, channel 0 for Left and channel 1 for Right.
// midSide (mid) is called for Mid (true) or Side (false) on stereo, where the caller encodes them.
// On mono, Left and Right are the same signal and it is all Mid: Right and Mid take channel 0, and
// Side has nothing to play on, so nothing is called and the part has no channels.
template <typename Channel, typename MidSide>
int routePlacement (StereoPlacement placement, int numChannels, Channel channel, MidSide midSide)
{
    switch (placement)
    {
        case StereoPlacement::Stereo:
            for (int ch = 0; ch < numChannels; ++ch)
                channel (ch, ch);
            return numChannels;
        case StereoPlacement::Left: channel (0, 0); return 1;
        case StereoPlacement::Right: channel (0, numChannels > 1 ? 1 : 0); return 1;
        case StereoPlacement::Mid:
            if (numChannels > 1)
                midSide (true);
            else
                channel (0, 0);
            return 1;
        case StereoPlacement::Side:
            if (numChannels < 2)
                return 0;
            midSide (false);
            return 1;
    }
    return 0;
}

// What a part routePlacement selected plays back on output channel ch of numChannels, decoded on its
// own (the other part silent): part (p) is the part's channel p at the sample. A Stereo part plays
// its own channel, a Left or Right part the channel it came from and silence on the other, a Mid
// part both, and a Side part the left and, inverted, the right (Left = Mid + Side, Right = Mid - Side).
// Silence on a third channel and on, and for Side on mono, which has no part.
template <typename Part>
float placedSample (StereoPlacement placement, int numChannels, int ch, Part part)
{
    float sample = 0.0f;
    routePlacement (
        placement,
        numChannels,
        [&] (int p, int source) {
            if (source == ch)
                sample = part (p);
        },
        [&] (bool mid) {
            if (mid)
                sample = ch < 2 ? part (0) : 0.0f;
            else
                sample = ch == 0 ? part (0) : ch == 1 ? -part (0) : 0.0f;
        });
    return sample;
}

} // namespace eq1
