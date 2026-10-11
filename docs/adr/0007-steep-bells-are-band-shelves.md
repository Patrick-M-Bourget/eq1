# A steep Bell is the band-pass transform of the Butterworth Low Shelf

Status: Proposed (#19). The maintainer picks a candidate; the Engine work follows the decision.

Every Bell is one second-order section today (12 dB/oct), and `band<n>_slope` is stored for a Bell but ignored (ADR 0003). Pro-Q 4 gives the Bell a Slope from 12 to 96 dB/oct. We propose that a Bell of Slope S is the band-pass transform p = Q·(s + 1/s) of the Butterworth Low Shelf of order S / 12: the Low Shelf target eq1 already has, with Gain at Frequency, 0 dB far away, and half the Gain in dB at the two points 1/Q of Frequency apart. At 12 dB/oct this is exactly today's Bell, (s² + s·A/Q + 1) / (s² + s/(A·Q) + 1), because a first-order shelf is fixed by those three values.

What a steeper Slope changes, at Q 2 and +12 dB (dB above Frequency; the curve is symmetric in log frequency):

| Slope | ¼ octave | ½ octave | 1 octave | 2 octaves | Biquads per Band |
|---|---|---|---|---|---|
| 12 | 7.83 | 4.24 | 1.47 | 0.28 | 1 |
| 24 | 9.38 | 2.74 | 0.19 | 0.01 | 2 |
| 48 | 11.19 | 0.90 | 0.00 | 0.00 | 4 |
| 96 | 11.95 | 0.06 | 0.00 | 0.00 | 8 |

Fixed at every Slope: Gain at Frequency, 0 dB at DC, and the half-Gain points, 1/Q of Frequency apart. What changes: the top flattens and the skirts outside the half-Gain points fall faster, as an order S / 12 transition on each side, the same order the Notch's Slope / 12 gives. The skirts fall monotonically and never pass Gain or 0 dB.

## Measurements

`python3 tools/filter-lab/filterlab.py bell-butterworth --q 0.1 0.71 2 10 40` (and `bell-chebyshev`), over Frequency 20 Hz to 20 kHz, Gain ±3 to ±30 dB, Q 0.1 to 40, at 44.1, 48 and 96 kHz, at 200 points from 10 Hz to Nyquist. Each figure is the worst error at 24 to 96 dB/oct, louder / quieter than the target for a boost (a cut mirrors it), as a share of |Gain|, the Bell test's measure. Today's Bell test allows 10%, 20% and 35%.

| Banded by | Frequency / Nyquist | Today's Bell | Butterworth (proposed) | Chebyshev I |
|---|---|---|---|---|
| Frequency | up to 0.45 | 4.3 / 8.3 | 43.3 / 9.7 | 27.7 / 12.2 |
| Frequency | up to 0.73 | 13.8 / 9.3 | 20.0 / 15.4 | 14.2 / 12.9 |
| Frequency | up to 0.91 | 28.2 / 10.2 | 48.1 / 18.6 | 25.4 / 14.5 |
| Upper half-Gain point | up to 0.45 | 1.2 / 4.8 | 7.9 / 5.2 | 5.8 / 3.9 |
| Upper half-Gain point | up to 0.73 | 3.4 / 7.6 | 12.9 / 12.5 | 9.0 / 10.6 |
| Upper half-Gain point | up to 0.91 | 9.0 / 9.3 | 21.7 / 18.6 | 17.0 / 13.4 |
| Upper half-Gain point | above 0.91 | 28.2 / 10.2 | 48.1 / 18.3 | 27.7 / 14.5 |

Banded by Frequency, the worst cases are a wide Bell (Q 0.1, Frequency 2–5 kHz) whose steep upper skirt sits near Nyquist, and a narrow one (Q 10) at 20 kHz and 44.1 kHz. Banded by where the upper half-Gain point sits, which is where a steep skirt meets Nyquist, the Butterworth candidate is within today's Bell limits up to 0.91. So the Engine's test for steep Bells should band by the upper half-Gain point, not by Frequency.

The design: each section of the transformed shelf is a matched section (`docs/dsp/filter-design.md`, "Method") at its own poles' natural frequency. A section whose natural frequency is above 0.95 × Nyquist is held there, as a steep Band Pass's upper sections are: its poles at the hold, with the Q that makes them as loud there, relative to DC, as the analog poles are, and its zeros fitting the analog section at DC, 0.9 × the hold, and Nyquist. Without the hold, wide and high Bells were up to 225% off.

## Considered Options

- **Chebyshev type I band shelf** (in the lab as `bell-chebyshev`, ripple 5% of |Gain| across the top). Its numbers are a little better, and its skirts are steeper for the same sections. Rejected:
  - At even orders (24, 48, 96 dB/oct) Frequency sits on a ripple trough, so either the ripple peaks pass Gain (the lab's choice: by 1.5 dB at +30 dB) or the curve misses Gain at Frequency.
  - The ripple is a new constant that no control sets.
  - Its sections reach Q 1,430 at 96 dB/oct and Q 40, against 254 for the Butterworth and 225 for today's Bell at Q 40 and +30 dB.
  - It has no fractional order: T_N for a non-integer N isn't a polynomial, and the trough at Frequency flips with the order's parity.
- **A High Shelf and a Low Shelf at the half-Gain points**, less the Gain once. Rejected: at 12 dB/oct it isn't today's Bell, and a narrow one never reaches Gain, because the two shelves overlap.
- **Chebyshev type II or elliptic band shelves.** Rejected: their ripple lies in the 0 dB region, so they colour the whole spectrum outside the Bell.
- **A cascade of identical Bells sharing the Gain.** Rejected: it narrows the top but the skirts still fall at 12 dB/oct.

## Fractional Slopes (#18)

The Butterworth candidate extends to fractional Slopes the way the Low Shelf does: a Bell of Slope S is the band-pass transform of the Low Shelf at S / 2 dB/oct (an 18 dB/oct Bell from the 9 dB/oct Low Shelf). The transform only remaps frequency, so it carries any of #18's candidates over as they are: a dB blend of neighbouring orders, or a partial section. The Chebyshev candidate doesn't extend (above).

## Consequences

- Sections: Slope / 12 biquads, 8 at 96 dB/oct, within the 16-section limit. To keep a Slope sweep from changing the section count mid-glide, a Bell always takes 8 biquads, the unused ones the identity, as a resonant shelf always takes two.
- 12 dB/oct is today's Bell exactly, coefficient for coefficient, so Bells in saved sessions and Presets don't change.
- Until #18 is decided, a Bell's Slope rounds to the nearest 12 dB/oct, as Notch's does.
- Open for the Engine step:
  - The hold isn't continuous: as a section's natural frequency crosses 0.95 × Nyquist, its match point jumps from the hold to 0.9 × the hold, and in isolation a section's response jumps by up to 19 dB at one frequency there. A Frequency sweep across it would click. Sliding the match point with the Q's reduction made wide Bells 80–90% off; Band Pass's blend of magnitudes (`docs/dsp/filter-design.md`, "Above 24 dB/oct") is the precedent to try.
  - The test's tolerances, banded by the upper half-Gain point.
  - Whether Detection and Solo keep the Bell's region, a second-order Band Pass as wide as Q, at every Slope (proposed: yes).
- Whether Pro-Q 4's steep Bell is this curve is unknown: no Pro-Q measurement was made. The flat top and fixed half-Gain width are eq1's own choice.

## Draft for `docs/dsp/filter-design.md`

To replace "Bell" in "Analog targets" (the first entry, and the "always one second-order section" entry):

> - **Bell:** the band-pass transform p = Q·(s + 1/s) of the Butterworth Low Shelf of order Slope / 12, with s normalised to Frequency (ADR 0007). So Frequency has the full Gain, the half-Gain points are 1/Q of Frequency apart, and each skirt is a transition of order Slope / 12. At 12 dB/oct it is (s² + s·A/Q + 1) / (s² + s/(A·Q) + 1), with A = 10^(Gain/40). Every Slope takes 8 biquads, the unused ones the identity.

To add under "Method":

> - **Bell sections:** each pair of zero and pole quadratics from one shelf root is one section, matched at its poles' natural frequency, designed as a boost and inverted for a cut. A section above 0.95 × Nyquist is held there: poles at the hold, with the Q that makes them as loud there, relative to DC, as the analog poles are, and zeros fitting the analog section at DC, 0.9 × the hold, and Nyquist.
