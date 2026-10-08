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

## Detection

When Detection Range is Band, a Dynamic Band's detector hears only the region its Shape works on. Solo plays the same region. Pro-Q 4 doesn't document this, so these regions are eq1's own:

- **Bell:** around Frequency, as wide as Q.
- **Low Shelf:** below Frequency.
- **High Shelf:** above Frequency.
- **Tilt Shelf and Flat Tilt:** the whole spectrum, unfiltered, because they affect all of it. Free Detection Range narrows it.
- **Low Cut, High Cut (Solo only):** the content being removed.
- **Notch, Band Pass, All Pass (Solo only):** around Frequency, as wide as Q.

Live Gain never goes beyond ±30 dB, whatever the Gain, Dynamic Range and Gain Scale.

**Solo** plays the main input, before the EQ, through a filter for the Band's region above (`engine/src/Solo.h`), so you hear what the Band works on, not its Gain:

- **Bell, Notch, Band Pass, All Pass:** a second-order Band Pass at Frequency, its −3 dB points 1/Q of Frequency apart.
- **Low Shelf:** a 12 dB/oct High Cut at Frequency. **High Shelf:** a 12 dB/oct Low Cut.
- **Tilt Shelf, Flat Tilt:** the input unfiltered.
- **Low Cut, High Cut:** the opposite Cut at the same Slope and Brickwall, at least 6 dB/oct, with Q 0.71 whatever the Cut's own Q.
- **Stereo Placement:** the region of the part the Band processes, decoded back to where it came from; the rest is silent. A Left Band's Solo is the left channel's region and a silent right; a Side Band's Solo on mono is silent.
- **Moving Solo** to another Band, or changing the Soloed Band's Stereo Placement, fades the old Solo out before the new one fades in. A Bypassed Band still Solos its region.
