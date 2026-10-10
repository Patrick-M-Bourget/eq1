#include "DisplayRange.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace eq1
{

HeardGains heardGains (const Settings& settings)
{
    HeardGains gains;
    for (size_t i = 0; i < gains.size(); ++i)
        if (const auto& band = settings.bands[i]; band.inUse && hasGain (band.shape))
            gains[i] = scaledByGainScale (band, settings.gainScale).gain;
    return gains;
}

int fittedDisplayRangeDb (int rangeDb, const HeardGains& seen, const HeardGains& now)
{
    const auto fits = [] (double gain, int range) { return std::abs (gain) <= range; };
    bool changedBeyond = false;
    for (size_t i = 0; i < now.size(); ++i)
    {
        const bool changed = now[i] && (! seen[i] || ! juce::exactlyEqual (*seen[i], *now[i]));
        changedBeyond = changedBeyond || (changed && ! fits (*now[i], rangeDb));
    }
    if (! changedBeyond)
        return rangeDb;
    for (int range : { 6, 12 })
        if (std::all_of (now.begin(), now.end(), [&] (const auto& gain) { return ! gain || fits (*gain, range); }))
            return range;
    return 30;
}

} // namespace eq1
