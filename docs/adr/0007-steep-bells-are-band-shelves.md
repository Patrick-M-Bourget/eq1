# A steep Bell is the band-pass transform of the Butterworth Low Shelf

Status: Accepted (#19): the Butterworth band shelf, with the steep-Bell test banded by the upper half-Gain point; Detection and Solo keep the 12 dB/oct region at every Slope. Continuity near Nyquist: above the hold, a section's match point slides from the hold to 0.9 × the hold.

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
| Frequency | up to 0.45 | 4.3 / 8.3 | 66.1 / 9.7 | 27.7 / 12.2 |
| Frequency | up to 0.73 | 13.8 / 9.3 | 20.0 / 15.4 | 14.2 / 12.9 |
| Frequency | up to 0.91 | 28.2 / 10.2 | 30.4 / 18.6 | 20.1 / 14.5 |
| Upper half-Gain point | up to 0.45 | 1.2 / 4.8 | 7.9 / 5.2 | 5.8 / 3.9 |
| Upper half-Gain point | up to 0.73 | 3.4 / 7.6 | 12.9 / 12.5 | 9.0 / 10.6 |
| Upper half-Gain point | up to 0.91 | 9.0 / 9.3 | 21.7 / 18.6 | 17.0 / 13.4 |
| Upper half-Gain point | above 0.91 | 28.2 / 10.2 | 66.1 / 18.3 | 27.7 / 14.5 |

Banded by Frequency, the worst cases are wide Bells whose steep upper skirt sits near or past Nyquist: Q 0.1 at 24 dB/oct, and Q 2 at 96 dB/oct. Banded by where the upper half-Gain point sits, which is where a steep skirt meets Nyquist, the Butterworth candidate is within today's Bell limits up to 0.91. So the Engine's test for steep Bells should band by the upper half-Gain point, not by Frequency.

The design: each section of the transformed shelf is a matched section (`docs/dsp/filter-design.md`, "Method") at its own poles' natural frequency. A section whose natural frequency is above 0.95 × Nyquist is held there, as a steep Band Pass's upper sections are: its poles at the hold, with the Q that makes them as loud there, relative to DC, as the analog poles are, and its zeros fitting the analog section at DC, a match point and Nyquist. The match point is the hold while the natural frequency is at the hold, and slides smoothly (in log frequency) to 0.9 × the hold as the natural frequency rises a quarter tone above it (see "Consequences"). Without the hold, wide and high Bells were up to 225% off.

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
- Until #18 is built, a Bell's Slope rounds to the nearest 12 dB/oct, as Notch's does.
- The hold is continuous: the match point slides from the hold to 0.9 × the hold over the quarter tone above it, eased in and out (a smoothstep in log frequency). A match point that jumps from the hold to 0.9 × the hold as the natural frequency crosses it (the lab's `hold` candidate) would click in a Frequency sweep. The poles were already continuous there: the held Q equals the analog Q at the hold. Q and Gain reach the design only through each section's shape and natural frequency, which they move continuously, so their sweeps cross the hold the same way; Slope still moves in whole steps of 12 dB/oct until #18.
  - Continuity: `python3 tools/filter-lab/filterlab.py bell-continuity --candidates hold slide` (and `clamp`, `blend`, `slide-blend`) measures the largest change in the response, at 265 points from 10 Hz to Nyquist, between two Frequencies 0.1 cent apart, straddling each section's crossing of the hold and 8 points across the quarter tone above it, over Slopes 12–96 dB/oct, Q 0.025–40, Gain ±3, ±12 and ±30 dB, at 44.1, 48 and 96 kHz, Frequency up to 30 kHz. For scale, the same step on an octave grid of Frequencies changes the response by up to 0.11–0.31 dB (12–96 dB/oct; a Q 40 Bell's skirt near Nyquist moves that fast).
  - The jump (`hold`): 23–38 dB at 24–96 dB/oct with Frequency up to Nyquist, 3–24 dB above it, the same at any step size. The slide: at most 0.32 dB up to Nyquist, as smooth as the grid, and shrinking with the step. Above Nyquist one case reaches 3.5 dB per 0.1 cent (60 dB/oct, Q 0.71, +30 dB, Frequency 28.9 kHz at 44.1 kHz), still shrinking tenfold with a tenfold smaller step: a notch the jump has there too (below).
  - Tolerance: `python3 tools/filter-lab/filterlab.py bell-candidates --candidates hold slide --q 0.1 0.71 2 10 40 --at-crossings`, the measurement above at 24–96 dB/oct, adding Frequencies 0.1 cent either side of each crossing. The slide leaves the bands this ADR sets as they were: 7.9 / 5.2, 13.7 / 12.5 and 21.7 / 18.6 (% of |Gain|, louder / quieter, against 10, 20 and 35%). Above 0.91, where no limit is set, it is 81.6 / 25.7 against the jump's 81.5 / 27.6. "Measurements" reads lower (12.9 up to 0.73, 66.1 / 18.3 above 0.91) because its grid has neither the crossings nor the 36, 60, 72 and 84 dB/oct Slopes.
  - Rejected: clamping every section's match point at 0.9 × the hold, held or not, is continuous but changes by up to 5.8 dB per 0.1 cent, a near-jump: a Q 85 section near the hold, matched that far below its peak, makes the zero fit ill-conditioned; it also took a narrow Bell above 0.91 to 78 / 77%. Band Pass's blend of magnitudes (`docs/dsp/filter-design.md`, "Above 24 dB/oct") adds nothing on its own (with that clamp, the same 5.8 dB) or on top of the slide (0.22 dB instead of 0.32, quieter above 0.91 30.0% instead of 25.7%), so the slide goes without it. A slower slide is worse on both counts: over a major third, Q 10 sections changed by up to 0.8 dB per 0.1 cent and wide Bells above 0.91 were up to 92% louder. Sliding with the Q's reduction made wide Bells 80–90% off.
- Above 0.91 × Nyquist, where no tolerance is set, the matched zero fit can fail: with Frequency above Nyquist a section matched near the hold can put a notch in the lower skirt, 40 dB below the target at worst in a scan of Frequency from 0.91 × Nyquist to 30 kHz (84 dB/oct, Q 0.71, +30 dB, Frequency 27.3 kHz at 44.1 kHz, at 11.3 kHz), with the jump and with the slide alike. #19 should check it.
- The steep-Bell test's tolerances are banded by the upper half-Gain point.
- Detection and Solo keep the Bell's region, a second-order Band Pass as wide as Q, at every Slope.
- Whether Pro-Q 4's steep Bell is this curve is unknown: no Pro-Q measurement was made. The flat top and fixed half-Gain width are eq1's own choice.

## Draft for `docs/dsp/filter-design.md`

To replace "Bell" in "Analog targets" (the first entry, and the "always one second-order section" entry):

> - **Bell:** the band-pass transform p = Q·(s + 1/s) of the Butterworth Low Shelf of order Slope / 12, with s normalised to Frequency (ADR 0007). So Frequency has the full Gain, the half-Gain points are 1/Q of Frequency apart, and each skirt is a transition of order Slope / 12. At 12 dB/oct it is (s² + s·A/Q + 1) / (s² + s/(A·Q) + 1), with A = 10^(Gain/40). Every Slope takes 8 biquads, the unused ones the identity.

To add under "Method":

> - **Bell sections:** each pair of zero and pole quadratics from one shelf root is one section, matched at its poles' natural frequency, designed as a boost and inverted for a cut. A section above 0.95 × Nyquist is held there: poles at the hold, with the Q that makes them as loud there, relative to DC, as the analog poles are, and zeros fitting the analog section at DC, a match point, and Nyquist. The match point slides from the hold to 0.9 × the hold as the natural frequency rises a quarter tone above the hold (a smoothstep in log frequency), so a Frequency, Q or Gain sweep is continuous across the hold.
