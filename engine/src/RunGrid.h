#pragma once

namespace eq1
{

// The Engine's grid: from prepare() on, the audio is cut into runs of this many samples, whatever the
// host's blocks. A Band re-designs its filter and its dynamics move once a run, the glides they step
// skip by a run at once, and the Output Level's RMS window moves on a run at a time.
inline constexpr int runSamples = 16;

} // namespace eq1
