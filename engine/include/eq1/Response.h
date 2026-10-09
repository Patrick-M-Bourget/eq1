#pragma once

#include "eq1/Settings.h"

namespace eq1
{

// The response the Engine applies, for drawing the EQ curve: computed from the same filter design
// the Engine runs, so the display and the sound agree. Safe to call from any thread; it doesn't
// allocate. Stereo Placement is not part of it: it is the curve on the part of the signal each Band
// processes.

// The magnitude in dB one Band applies at frequency (Hz), whether or not it is in use or Bypassed. Its
// Gain is taken as given, held to +/-30 dB: scaledByGainScale() gives the Band as Gain Scale plays it.
double bandResponseDb (const BandSettings& band, double frequency, double sampleRate);

// The same at count frequencies at once, into db: the Band's filter is designed only once.
void bandResponseDb (const BandSettings& band, const double* frequencies, double* db, int count, double sampleRate);

// The magnitude in dB of every Band in use and not Bypassed, together, under Gain Scale.
double responseDb (const Settings& settings, double frequency, double sampleRate);

// Auto Gain's estimate: the gain in dB that brings pink noise from 20 Hz to 20 kHz, through the
// static curve of every Band in use and not Bypassed under Gain Scale, back to the average power it
// went in at. Dynamics and Stereo Placement are not part of it: the curve is the one drawn.
double autoGainDb (const Settings& settings, double sampleRate);

} // namespace eq1
