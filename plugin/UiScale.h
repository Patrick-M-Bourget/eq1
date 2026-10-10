#pragma once

#include <algorithm>
#include <array>

namespace eq1::uiScale
{

// The UI Scales the editor offers, in percent.
inline constexpr std::array<int, 5> percents { 75, 100, 125, 150, 200 };
inline constexpr int defaultPercent = 100;

constexpr bool isOffered (int percent) { return std::find (percents.begin(), percents.end(), percent) != percents.end(); }

} // namespace eq1::uiScale
