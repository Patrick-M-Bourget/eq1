#pragma once

#include "eq1/Settings.h"

#include <array>
#include <optional>

namespace eq1
{

// The Heard Gain (its Gain under Gain Scale) of each Band Slot the Display Range has to fit: one in
// use whose Shape has Gain, Bypassed or not. Nothing for the others.
using HeardGains = std::array<std::optional<double>, numBandSlots>;
HeardGains heardGains (const Settings& settings);

// The Display Range once the Heard Gains have gone from seen to now. When a Band's Heard Gain has
// changed to strictly beyond rangeDb, the smallest of 6, 12 and 30 that fits every Band, or 30 when
// none does; otherwise rangeDb, even with a Band beyond it: it never zooms back in by itself.
int fittedDisplayRangeDb (int rangeDb, const HeardGains& seen, const HeardGains& now);

} // namespace eq1
