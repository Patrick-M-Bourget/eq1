# Filter design

How each Shape becomes a decramped digital filter (ADR 0001), what analog curve it must match, and how closely the Engine tests hold it to that curve. The code is in `engine/src/MatchedDesign.cpp` and `engine/src/ShapeDesign.cpp`. Try a new or changed design in `tools/filter-lab/filterlab.py` before writing C++: it runs the same design and the same error measure in Python.

## Method

- **Sections:** every Shape is a cascade of matched sections, at most 16 per Band.
- **Matching a section:**
  - The poles are the matched z-transform of the analog poles.
  - The numerator is chosen so the magnitude equals the analog section's at DC, at the reference frequency and at Nyquist.
  - A first-order section matches at DC and Nyquist only.
  - Above 0.98 × Nyquist, the pole frequency and the match point are held just below Nyquist.
- **Design direction:** each section is designed in the direction whose poles sit at or below Frequency, where the matched z-transform is accurate, and inverted for the other direction. Bells and Low Shelves are designed as boosts; High Shelves as cuts.
- **Low Cut sections:** the three-point match can't place a double zero exactly at DC, which leaves the stopband tens of dB too loud. So a Low Cut section keeps the matched poles, fixes its zeros at DC, (1 − z⁻¹)² or (1 − z⁻¹), and is scaled to the analog magnitude at Frequency.
- **High Cut sections:** each is matched at its damped natural frequency, Frequency × √(1 − 1/(4Q²)) (at least 0.1 × Frequency, for sections with Q below 0.5), held at or below half Nyquist. Matching at Frequency instead bulges the passband by up to 35 dB near Nyquist at Brickwall; this way a High Cut rolls off early there instead of boosting.
- **Band Pass sections:** each Butterworth pole pair becomes a lower and an upper pole pair. The lower one is designed as a Low Cut section (zeros at DC), with its gain the geometric mean of the gains matching at its own natural frequency and at Nyquist. The upper one is designed as a High Cut section, matched at no more than 0.9 of its natural frequency: matched right at the peak of a very sharp section, the poles' tiny error shows up as tens of dB of gain. Plain band-pass sections fail wide Band Passes, whose two pole pairs sit far apart. Every section is designed around its own natural frequency, so poles above Nyquist are held just below it.
  - **Above 24 dB/oct:** held that way, each upper section bumps up a little at the hold, and the sections' small errors all lean the same way, so a 96 dB/oct Band Pass would read up to 18 dB too loud near Nyquist. A steep Band Pass changes two things:
    - **Held upper sections:** an upper section whose natural frequency is above 0.95 × Nyquist has its poles placed there, with the Q that makes it as loud there, relative to DC, as the analog section is (never a higher Q). Its zeros fit a blend of the analog section's magnitude and the held section's own, weighted by √(1 − held Q / analog Q): the analog section's where the Q isn't lowered, so the design is continuous as Frequency or Q crosses the hold, and the held section's as it is lowered most, so the zeros don't reach for the analog peak above Nyquist.
    - **Whole-cascade gain:** the cascade's gain is set to the target's at Frequency, or at the hold if Frequency is above it.
  - Band Passes at 24 dB/oct and below keep the plain design. Lowering the Q alone, or setting the gain alone, each left one region far outside the limits below, and holding at 0.98 × Nyquist left the top region 2–3 dB too loud; holds from 0.94 to 0.96 all pass.
- **Notch sections:** the zeros are fixed on the unit circle exactly at Frequency, and the gain matches the analog section at DC.
- **All Pass:** bilinear, prewarped at Frequency: the one Shape that isn't matched. Its magnitude is flat by construction, so it can't cramp (ADR 0001), and the bilinear phase is exact at Frequency, where matched poles were 16° off at order 1 by 0.45 × Nyquist. A digital all-pass reaches −180° × order at Nyquist while the analog one only gets there at infinity, so near Nyquist its phase runs ahead.
- **Why one biquad can't be exact:** a biquad's response is flat at Nyquist, while a high or wide analog curve is still sloping there. A single section cannot match it exactly near Nyquist. RBJ bilinear Bells miss by 85–100% on the Bell test's grid.

## Analog targets

- **Bell:** (s² + s·A/Q + 1) / (s² + s/(A·Q) + 1), with s normalised to Frequency and A = 10^(Gain/40).
- **Low Shelf, High Shelf, Tilt Shelf:**
  - Butterworth shelves of order Slope / 6, in whole steps of 6 dB/oct up to 96 dB/oct.
  - The zeros sit on a circle of radius g^(1/2N) and the poles on the reciprocal circle, so Frequency is where a shelf reaches half its Gain in dB.
  - A High Shelf is the Low Shelf mirrored in frequency (s → 1/s).
  - Tilt Shelf is the High Shelf moved down by half its Gain, so it passes 0 dB at Frequency.
- **Shelf Q:** Q sets the corner resonance.
  - Each second-order section's Q is the Butterworth one scaled by (Q / 0.71)^(1 / sections). So Q 0.71 is a plain Butterworth shelf, and a resonant 96 dB/oct shelf rings about as much as a 12 dB/oct one.
  - Scaling every section by Q / 0.71 instead gives peaks of about 150 dB at 96 dB/oct.
  - A 6 dB/oct shelf has no second-order section, so Q has no effect on it.
- **Flat Tilt:**
  - A straight line of Gain / 10 dB per octave through 0 dB at Frequency. Gain is the tilt across the 10 octaves from 20 Hz to 20 kHz.
  - It is built from 13 first-order shelves, one per octave from 5 Hz to 40 kHz.
  - It is set to 0 dB at Frequency on the digital response.
  - It ignores Slope and Q.
- **Low Cut, High Cut:**
  - Butterworth of order Slope / 6, so 96 dB/oct is order 16 (8 second-order sections). Frequency is the −3 dB point.
  - Q scales each second-order section's Q as for Shelves: (Q / 0.71)^(1 / sections).
  - 0 dB/oct passes the signal unchanged.
  - **Brickwall** is Butterworth of order 32 (16 second-order sections, about 192 dB/oct), the most the 16-section limit allows. Elliptic designs were rejected because their stopband zeros can't be matched near Nyquist; linear-phase FIRs because they add latency (ADR 0001).
- **Band Pass:** a band-pass transform (s → Q·(s + 1/s)) of a Butterworth low-pass of order Slope / 6 (one second-order section per order, so 16 at 96 dB/oct), so each skirt falls at Slope dB/oct. Q sets the width: 1/Q of Frequency between the −3 dB points. 0 dB/oct passes the signal unchanged.
- **Notch:** a band-stop transform of a Butterworth low-pass of order Slope / 12 (one second-order section per order), so 12 dB/oct is the standard second-order notch. Q sets the width.
- **All Pass:** Butterworth poles of order Slope / 6, with zeros mirrored across the jω axis, so the phase at Frequency is −90° × order. Q scales the sections as for Cuts. Magnitude is flat; it is tested on phase.
- **Bell:** always one second-order section (12 dB/oct) until #19 decides a steeper target.
- **Slopes between whole orders:** until #18 decides a fractional target, a Slope is rounded to the nearest whole order (to the nearest 6 dB/oct, or 12 dB/oct for Notch). So a Cut or Band Pass below 3 dB/oct passes the signal unchanged. This rule is temporary.

## Test tolerances

How far the Engine's response may stray from the analog target in `tests/engine/ShapeResponseTest.cpp`, by where Frequency sits relative to Nyquist. Cuts, Band Pass and Notch are read against the target shifted up to 1/12 octave either way: **louder** is how far the response rises above the highest of those (or above −60 dB, where the target is lower), **quieter** how far it falls below the lowest of them, where that is above −24 dB. Louder matters most: the skirt then passes what it should remove.

**Band Pass** (louder / quieter, dB):

| Frequency / Nyquist | Up to 24 dB/oct | Above 24 dB/oct | Above 24 dB/oct, measured |
|---|---|---|---|
| up to 0.45 | 0.5 / 1.0 | 2.5 / 5.5 | 1.0 / 4.9 |
| up to 0.73 | 2.0 / 2.5 | 3.0 / 6.0 | 0.7 / 2.8 |
| up to 0.91 | 5.0 / 2.0 | 5.0 / 9.0 | 3.1 / 6.0 |

The measured column is the worst case in the filter lab over the test's grid (Frequency 20 Hz to 20 kHz, Q 0.1, 0.71, 2, 10 and 40, at 44.1, 48 and 96 kHz), at 200 points from 10 Hz to Nyquist: `python3 tools/filter-lab/filterlab.py band-pass --orders 5 6 7 8 9 10 11 12 13 14 15 16 --q 0.1 0.71 2 10 40`.

The other Shapes' limits are in the test, beside each Shape's check.

## Detection

When Detection Range is Band, a Dynamic Band's detector hears only the region its Shape works on. Solo plays the same region. Pro-Q 4 doesn't document this, so these regions are eq1's own:

- **Bell:** around Frequency, as wide as Q.
- **Low Shelf:** below Frequency.
- **High Shelf:** above Frequency.
- **Tilt Shelf and Flat Tilt:** the whole spectrum, unfiltered, because they affect all of it. Free Detection Range narrows it.
- **Low Cut, High Cut (Solo only):** the content being removed.
- **Notch, Band Pass, All Pass (Solo only):** around Frequency, as wide as Q.

Live Gain never goes beyond ±30 dB, whatever the Gain, Dynamic Range and Gain Scale.

## Dynamics

How a Dynamic Band moves its Live Gain (`engine/src/Dynamics.cpp`). Pro-Q 4 only says its timing and knee depend on the material, so the numbers below are eq1's own:

- **Detection signal:** the Detection Source (the main input before the EQ, or the Sidechain), through the Detection Range, on the part of the signal the Band processes: Left, Right, Mid or Side, or both channels for a Stereo Band. A Side Band on a mono main input hears nothing and doesn't move.
- **Sidechain:** a stereo Sidechain follows the main input's rules, so a Mid or Side Band hears its Mid or Side. A mono Sidechain is the detection signal for every Stereo Placement, Side included, so a mono kick on the Sidechain always works (eq1's own rule; Pro-Q 4 doesn't document it). With no Sidechain connected, External Bands hear silence: they don't move, and one already moved returns to Gain at its Release.
- **Detection Range:** Band uses the region filter below (the same one Solo plays). Free is a 24 dB/oct Butterworth Low Cut at its low limit followed by one High Cut at its high limit; limits set the wrong way round leave almost nothing to hear.
- **Level:** the detection signal's power, smoothed over 5 ms, in dB where a full-scale sine reads 0 dB. A Stereo Band takes the louder channel's level at each sample, and so applies one Live Gain to both.
- **Gain computer:** movement starts 3 dB below Threshold (a soft knee) and rises smoothly, about 2:1, to the full Dynamic Range at 2 × |Dynamic Range| + 3 dB above it. Live Gain is Gain plus the movement times Dynamic Range, held to ±30 dB.
- **Auto Threshold:** 4 dB above the mean level of the region over about the last 2 s (the mean of all it has heard, until it has heard 2 s; it starts afresh whenever the Band becomes active, so it follows the material playing then). Levels below −80 dB don't count, so silence doesn't pull it down. Steady material rests below it; what stands out of the material moves the Band, at any overall level.
- **Auto Attack:** 20 ms just above Threshold, faster the further above it the level goes: 20 ms × 3 / (3 + overshoot in dB).
- **Auto Release:** 40 ms after a short burst, up to 500 ms after sustained movement, measured as the mean movement over about the last 0.5 s.
- **Attack and Release settings:** the Auto timing times 10^((setting − 50%) / 50%), so 0% is ten times faster and 100% ten times slower. 50% is Auto itself, and the timing still follows the material at every setting.
- **Timing:** the Engine runs on a grid of 16-sample runs that carries across host blocks. The gain computer moves at the end of each run, over the levels the run heard, with Auto Attack's loudest level and Auto Threshold taken over the whole run; the Band's filter glides to the result over the next run. A Dynamic Band so reacts up to one run (about 0.33 ms at 48 kHz) after its detection, with no latency, and sounds the same at every host block size.
- **Changes:** Dynamic Range, Dynamics Bypass and a Shape change to or from one without dynamics glide over about 50 ms, like a Band's own settings, so the Band doesn't click. Shapes without dynamics, Bands not in use and Bypassed Bands don't run their detector.

**Detection Level** is published for one metered Band (`Settings::meteredSlot`): the loudest level its detector compared with Threshold since the last read, the louder channel's for a Stereo Band. The metered Band's detector runs whatever its dynamics state (Dynamics Bypass, Dynamic Range 0), but only to measure: its movement, Auto Threshold and glides wait as they do unmetered, and it starts afresh when it becomes active, so metering never changes the sound. Only a Band in use, not Bypassed, whose Shape has dynamics is metered; otherwise, and when its detector has nothing to listen to, it reads the −150 dB floor.

**Detection Audition** plays a Dynamic Band's detection signal, after the Detection Range, instead of the output: the one detection channel on every output channel, or each channel's own for a Stereo Band on a stereo source. It crossfades in and out like Solo, runs the detector while held even when the Band isn't dynamic yet, and takes precedence over Solo.

**Solo** plays the main input, before the EQ, through a filter for the Band's region above (`engine/src/Solo.h`), so you hear what the Band works on, not its Gain:

- **Bell, Notch, Band Pass, All Pass:** a second-order Band Pass at Frequency, its −3 dB points 1/Q of Frequency apart.
- **Low Shelf:** a 12 dB/oct High Cut at Frequency. **High Shelf:** a 12 dB/oct Low Cut.
- **Tilt Shelf, Flat Tilt:** the input unfiltered.
- **Low Cut, High Cut:** the opposite Cut at the same Slope and Brickwall, at least 6 dB/oct, with Q 0.71 whatever the Cut's own Q.
- **Stereo Placement:** the region of the part the Band processes, decoded back to where it came from; the rest is silent. A Left Band's Solo is the left channel's region and a silent right; a Side Band's Solo on mono is silent.
- **Moving Solo** to another Band, or changing the Soloed Band's Stereo Placement, fades the old Solo out before the new one fades in. A Bypassed Band still Solos its region.

## Output

What follows the Bands (`engine/src/Output.h`, `engine/src/AutoGain.cpp`):

- **Gain Scale** multiplies each Band's Gain and Dynamic Range in dB, on Shapes with a Gain, before anything else: the Band glides to it as to a Gain change, and Live Gain stays within ±30 dB. The drawn curve uses the same scaled Gains.
- **Auto Gain** is −10·log₁₀ of the curve's mean power gain at 1,024 frequencies spaced evenly in log frequency from 20 Hz to 20 kHz (up to Nyquist below 40 kHz): pink noise has equal power per octave, so this brings pink noise over that range back to the average power it went in at. It uses each Band's Gain, scaled, without dynamics, and ignores Stereo Placement, as the drawn curve does. It is held to ±30 dB: Bands that leave almost nothing, such as Cuts that leave only a narrow band, would otherwise ask for a boost that blows up the rest of the signal. The tests hold it within 0.5 dB of a measured pink probe on the reference settings in `tests/engine/OutputTest.cpp`; it measures within 0.1 dB. The Engine works a new estimate out over about 40 ms of audio once the Bands change, and glides to it.
- **Output Gain, Output Pan, Phase Invert:** one 2×2 mix of the two channels, each of its four amounts gliding over about 50 ms, so every change, Pan Mode included, is click-free. Pan is a balance: the centre leaves both sides at unity and each step towards one side turns the other down linearly, to silence at the end. In M/S the left side is Mid and the right Side. On mono only Output Gain, Auto Gain and Phase Invert apply.
- **Global Bypass** crossfades over about 50 ms to the input as it came in. The Bands keep running underneath, so switching back is click-free too.
