# Fractional Slopes morph the cascade between neighbouring whole orders

**Status:** Accepted (#18): the partial cascade; beyond about an octave the stopband falls at the next whole order. At Q 0.71 to 40 the design the Engine builds has no jumps between nearby Slopes except one seam on Band Pass and Notch, which the Engine crossfades, and keeps Shelves within their tolerances. Since #196 that holds at every Q from 0.025 to 40, and at whole orders the design is the existing one: see "The design the Engine builds".

`band<n>_slope` is already continuous (ADR 0003), but the Engine rounds it to the nearest whole order. Pro-Q 4's Slope is continuous (for example 3.5 dB/oct). This ADR proposes the analog target for a Slope between whole orders. Bell's Slope is #19.

It covers the Shapes that round today: Low Shelf, High Shelf, Tilt Shelf, Low Cut, High Cut, Band Pass and Notch. Write the order as ν = Slope / 6 (Slope / 12 for Notch), with N = ⌊ν⌋ and t = ν − N.

## Proposal: the morphing cascade is the target

The cascade at order ν is built from order N + 1's sections, as follows:

- **Shared second-order sections:** each one that orders N and N + 1 both have moves its damping (1/Q) linearly with t, from order N's Butterworth value to order N + 1's. Q scaling is applied at both ends first.
- **The partial section:** the section that order N lacks grows in.
  - N even: a first-order pole comes in from infinity, `ts + 1`.
  - N odd: order N's real pole gains a second pole from infinity, `ts² + ds + 1`. The two real poles meet and become a pair before t reaches 1.
- **Corner:** the cascade is rescaled in frequency so Frequency stays at −3 dB, measured at Q 0.71. Below 6 dB/oct nothing is rescaled: the pole simply comes in from infinity. The rescale is three Newton steps (see "The design the Engine builds").
- **Shelves:** each prototype pole r gives a zero at r·rz^w and a pole at r/rz^w, with rz^(2Σw) = Gain. Here w is 1 for a whole root. For the root coming in from infinity, w rises from 0 to 1 by the point where it meets order N's real pole. So the root's step comes in gradually. The shelf is then rescaled so Frequency is at half the Gain in dB. A High Shelf is that Low Shelf mirrored (s → 1/s); unlike Butterworth's, it isn't the Low Shelf of −Gain moved up by Gain.
- **Band Pass and Notch:** the same prototype goes through their existing transforms. A real prototype pole gives one band-pass or notch section; a pair gives two. A Band Pass pair's two sections together carry the transform's gain, |p|²/Q², where p is the prototype pole (Butterworth's |p| is 1).

At t = 0 the cascade is exactly order N's Butterworth cascade, at every Q: the lab's whole-order designs match the existing ones within 10⁻¹² dB. Every coefficient moves continuously with Slope.

Why this one:

- **Accuracy:** it is the only candidate the matched design meets with whole-order accuracy in every region and at every Q. It is its own target, so all that is left is decramping error.
- **Cost:** it needs no sections beyond the next whole order's. At 96 dB/oct nothing changes: Cuts take 8 biquads, Shelves 16, Band Pass 16 and Notch 8.
- **Q:** it extends to Q the same way whole orders do.

What it costs:

- **Far stopband:** past roughly an octave beyond Frequency (where the partial pole sits), a Cut falls at the next whole order, 6⌈ν⌉ dB/oct, not at Slope. So Slope sets the steepness near Frequency.
- **Small bumps and reversals:** a curve can bump against its own direction by up to 0.44 dB on Cuts and Band Pass, 0.34 dB on Notch and 0.22 dB on Shelves. As Slope rises, a Shelf's steepness can briefly ease by up to 0.59 dB.

## Considered Options

All three candidates are realised by the same morphing cascade, so they cost the same. They differ in what the design is held to. The lab measures each one: `python3 tools/filter-lab/filterlab.py <shape> --slopes 15 21 87 --target blend|partial|formula`, and `filterlab.py slopes` for the analog behaviour. The full comparison table is in the PR that adds this ADR.

- **Blend the neighbouring whole orders in dB:** (1 − t) × order N's target + t × order N + 1's.
  - Strengths: exact at whole orders, always monotonic, never bumps, and the far stopband falls at exactly Slope.
  - Rejected for now because no cascade of whole sections has a fractional asymptote. Near Frequency, the morphing cascade comes within 0.3–0.5 dB of the blend for Cuts and within 2 dB for Shelves. But the High Cut is up to 17.7 dB too quiet at 3 and 9 dB/oct. Resonant Shelves miss by 41–86% of the span at Q 3 and 10, because a blend of two resonant shelves has two peaks.
  - Meeting it would need extra sections that carry the fractional slope out to −60 dB (a fractional tail). An analog check of a closed-form tail still missed by up to 3 dB, and the lab doesn't build one yet.
- **The Butterworth formula with a fractional exponent:** |H|² = 1/(1 + x^(2ν)); Shelves (g + x^(2ν))/(1/g + x^(2ν)).
  - Strengths: the same as blend, and within 0.1–0.2 dB of it near Frequency.
  - Rejected because it has no Q: it is defined at Q 0.71 only. Also, below 6 dB/oct a Cut goes to −3 dB everywhere as Slope approaches 0, a jump from passing the signal unchanged at 0.
- **Crossfading two whole-order cascades in amplitude.** Not measured. It costs two cascades, and where their phases differ it notches near Frequency.

## Consequences

- **The Engine's tests** compare against the morphing cascade's analog response, as they compare against Butterworth now. The test grids gain fractional Slopes.
- **Steep change after each whole order:** just past a whole order, the analog curve itself changes quickly, because the incoming pole sweeps through the audio band. For example, a Low Cut at 2 kHz moves up to 0.74 dB per 0.01 dB/oct. Gliding Slope is still continuous.
- **No parameter changes.** Sessions saved with a rounded Slope sound different once fractional Slopes ship. That is unreleased behaviour, so there is no migration.

## The design the Engine builds

The lab's `continuous_design` is the design the Engine builds, and the lab's fractional reports use it. Whole orders keep today's designs: `python3 tools/filter-lab/filterlab.py slope-gaps whole --jobs 4` checks every Shape at every whole order over the seams' grid below and fails above 10⁻⁶ dB (worst 2.3 × 10⁻⁷ dB, rounding at 20 Hz and 96 kHz). Before #196 it failed: High Cut at low Q by 4.9 dB and the Shelves at Q 40 near Nyquist by 1.2 dB, once #195 moved the existing design's match point. `python3 tools/filter-lab/filterlab.py slope-gaps --jobs 4` reproduces every number below, for the lab's first design (`fractional_design`) and the one the Engine builds (`continuous_design`) (about 40 minutes on an idle machine; `slope-gaps rescale` alone takes seconds).

- **High Cut's sections:** every second-order section is one matched biquad, as at whole orders, whether its poles are real or a pair. Each is matched at its damped natural frequency, and at 0.1 × its natural frequency while its poles are real (#196). `fractional_design` designs a section with real poles as two first-order sections.
- **High Cut's partial section:** one matched biquad with exact poles, matched like the other sections. While its poles are real, the match point aims at the analog magnitude times the error two first-order sections would have there, weighted by 1 − c. Here c = 1 − (1 − near/far)²(1 − t): c is 0 as the pole comes in from infinity, and 1 where the poles meet or at the next whole order, whichever comes first (at low Q the poles don't meet before it; #196 added the (1 − t)). It moves smoothly where they meet. `fractional_design` uses two first-order sections, then switches to one biquad where the poles meet.
- **Shelves' resonant split:** each section's zeros' Q is reduced to the Q the same section has at Q 2. For whole orders, this is exactly the existing (2/Q)^(1/sections). The extra resonance is matched where the existing split matches it (`docs/dsp/filter-design.md`, "Method", resonant shelves' match point; #195). The partial section's split is weighted by t³, and by c while its roots are real. While they are real, it is matched like the High Cut's partial section, at Frequency.
- **Band Pass:** a pair's upper section carries the |p|²/Q² gain. `fractional_design` uses 1/Q², which is right only when |p| is 1, so its fractional passband falls by up to 7.7 dB, and jumps by that where the partial section's poles meet. The steep design (above 24 dB/oct) now comes in linearly from 24 to 30 dB/oct: the held upper sections' coefficients and the cascade's gain correction move from the plain design. A biquad's stable coefficients form a convex set, so every step in between is stable.
- **Corner rescale:** three Newton steps on the level in dB against ln(frequency), starting from the whole-order corner (x = 1). Each step evaluates the unscaled Q 0.71 cascade and its derivative at one frequency. The worst error against the lab's bisection is 7 × 10⁻⁵ cents for Cuts (every Slope 0.01 dB/oct apart) and 10⁻⁵ cents for Shelves (Gain ±0.1 to ±30 dB), under 10⁻⁶ dB on the response at Q 0.71 or 40. Two steps leave 0.4 cents for Cuts and 2.6 cents for Shelves (0.005 dB). A table isn't needed.
- **Crossfade where Band Pass and Notch poles meet:** the prototype's two real poles each give their own section, and the pair splits into a lower and an upper section (Band Pass) or two notch sections (Notch). The groupings differ, so the design jumps where the poles meet. That happens at 7.9, 21.1, 33.8, 46.2, 58.5, 70.7, 82.9 and 95.0 dB/oct on Band Pass, and at 15.9, 42.2, 67.5 and 92.4 dB/oct on Notch, whatever the Q. The jump reaches 18.1 dB on Band Pass (Q 10, 20 kHz at 44.1 kHz) and 1.6 dB on Notch (Q 0.1, near Nyquist).
  - A Slope change across one of these Slopes is a structure change, so the Engine crossfades it, as it crossfades every Slope change today. "Changing Slope does not click" crossfades a Low Shelf between 6 and 96 dB/oct, a far larger change.
  - The cost is stepping: a sweep from 0 to 96 dB/oct crossfades 8 times on Band Pass and 4 times on Notch, against 16 and 8 times today. Each time, Slope waits for one crossfade, about 50 ms.
  - Moving from 0 dB/oct to above it is a structure change too, from no sections to one. Every other Slope change can glide. Just above a whole order, the cascade can have more biquads than at the whole order, and the extra ones start as the identity.

Outcome per gap. The worst case covers 44.1, 48 and 96 kHz and Frequency up to 0.91 × Nyquist. Seams are the jump over 0.01 dB/oct beyond the analog target's own change, at Q 0.025, 0.1, 0.71, 2, 3, 10 and 40 (the Q parameter's whole range) and Gain ±3 and ±18. They are looked for at each whole order, where each partial section's poles meet and at Band Pass's 24 and 30 dB/oct, and everywhere else by a scan of every Slope 0.1 dB/oct apart from the Shape's lowest to 96 dB/oct, whose most outstanding step is halved down to 10⁻⁵ dB/oct. A seam stays as the step shrinks; elsewhere the jump shrinks with the step.

| Gap | First design | #161 | Now (#196) | Verdict |
|---|---|---|---|---|
| High Cut seam where the poles meet (Q 0.71 to 40) | 1.21 dB | 0.02 dB, shrinking with the step | 0.02 dB, shrinking | closed |
| Low Shelf seams (Q 0.71 to 40, at odd whole orders, where the pairs count changes) | 10.1 dB | 0.15 dB, shrinking | 0.09 dB, shrinking | closed |
| High and Tilt Shelf seams (Q 0.71 to 40, at whole orders) | 10.4 dB | 0.11 dB, shrinking | 0.13 dB, shrinking | closed |
| High Cut, Q 0.1 and 0.025 | — | 1.17 dB at 24.5 dB/oct (Q 0.1, 9 kHz at 96 kHz); 0.40 dB at 96 dB/oct (Q 0.1, 20 kHz at 96 kHz) | 0.03 dB elsewhere and 0.02 dB where the poles meet, shrinking; 0.81 dB at whole orders, shrinking to 0.000 (Q 0.025, 20 Hz at 96 kHz: the analog curve's own steep change just past a whole order) | closed |
| Shelves, Q 0.1 and 0.025, at whole orders | — | 1.15 dB at 24 dB/oct (Q 0.1, ±18 dB, 20 kHz at 44.1 kHz) | 0.03 dB, shrinking | closed |
| High Cut at Q 40, 20 Hz, 96 kHz | 1.28 dB, growing as the step shrinks | the same | the same | rounding, not a seam: see below |
| Band Pass at 24 dB/oct (steep design) | 7.51 dB | 0.01 dB, shrinking | 0.01 dB, shrinking | closed |
| Band Pass where the poles meet | 18.1 dB | 18.1 dB | 18.1 dB | crossfade covers it |
| Notch where the poles meet | 1.60 dB | 1.60 dB | 1.60 dB | crossfade covers it |
| Low Cut | 0.10 dB, shrinking | unchanged | unchanged | no seam |
| Resonant Low Shelf (Q above 2), up to 0.45 / 0.73 / 0.91 × Nyquist | 35.1% / 39.8% / 81.4% | 10.9% / 14.6% / 43.9% | 10.9% / 14.6% / 43.9% | inside (limits 12 / 15 / 45%) |
| Resonant High Shelf (Q above 2) | 20.6% / 40.5% / 141.9% | 11.2% / 16.1% / 43.9% | 11.2% / 13.2% / 43.9% | inside since #195's match point |
| Low Shelf, Q up to 2 | 10.3% / 10.8% / 43.8% | 10.3% / 11.1% / 43.8% | 10.3% / 10.9% / 43.8% | inside (limits 12 / 12 / 45%) |
| High Shelf, Q up to 2 | 10.5% / 14.0% / 55.9% | 10.5% / 11.1% / 46.1% | 10.5% / 11.1% / 46.1% | inside the test tolerance (share 46.1%) |
| Shelves, Q 0.1 and 0.025 (Slopes 9 to 93 dB/oct) | not measured | 5.1% / 4.3% / 38.2% | 5.1% / 5.4% / 39.9% | inside |
| Corner rescale | bisection (unbounded) | 3 Newton steps, 10⁻⁴ cents | unchanged | closed |

The first-design and "Now" columns are `python3 tools/filter-lab/filterlab.py slope-gaps seams shelves --jobs 4` on the #196 revision (48 minutes on a busy machine). The #161 column is the same command on that revision (`d1cf5fd`). The low-Q Shelf row is `filterlab.py low-shelf --slopes 9 15 21 33 45 69 93 --q 0.025 0.1` and the same for `high-shelf`, worst of both. Each remaining seam is either rounding or crossfaded, so the Slope crossfade triggers only where Band Pass's and Notch's poles meet, at the Slopes listed above whatever the Q (their prototype is always at Q 0.71): 8 times in a sweep from 0 to 96 dB/oct on Band Pass, 4 times on Notch, plus once leaving 0 dB/oct. High Cut and the Shelves need no crossfade at any Q.

How #196 closed the low-Q seams:

- **High Cut:** at low Q a section's poles are real. `_fractional_cut` designed a section with real poles as two first-order sections and one with a pair as one matched biquad, so the design jumped where a shared section's poles turn into a pair as Slope moves (1.17 dB at 24.5 dB/oct). Whole orders match every section as one biquad, so it also jumped at whole orders (0.40 dB at 96 dB/oct), and strayed from the whole-order design by up to 4.9 dB (Q 0.025, order 16). Every shared section is now one matched biquad, as at whole orders. The match target was the cause, not the analog target.
- **Shelves and High Cut's partial section:** at low Q the partial section's roots stay real up to the next whole order, so its weighting by c never reached 1 and the design jumped onto the whole order's (1.15 dB at 24 dB/oct). c now also reaches 1 at the next whole order: the weight is (1 − near/far)²(1 − t).
- **Accuracy at low Q:** High Cut, against the partial target (`filterlab.py high-cut --slopes 4.5 10.5 16.5 22.5 28.5 34.5 46.5 70.5 94.5 95.5 --q 0.025 0.1`), quieter by at most 0.47 / 1.04 / 4.06 dB up to 0.45 / 0.73 / 0.91 × Nyquist. It was 1.01 / 3.68 / 5.33 dB, over the 1.0 dB limit below 0.45 × Nyquist. The limits are 1.0 / 5.0 / 12.0 dB, and it is never louder. Q 0.71 to 40 are unchanged to 0.01 dB. The low-Q Shelves move by at most 1.7 points of the span.

The two checks the lab owed:

- **Band Pass reading:** the lab already reads Cuts, Band Pass and Notch as `ShapeResponseTest` does (`report_cut`: the target shifted ±1/12 octave, louder above the highest of the three or −60 dB, quieter below the lowest where that is above −24 dB), on a denser grid (200 points against the test's 64). The 3.8–4.5 dB "quieter" up to 0.45 × Nyquist comes from 87 dB/oct in `filterlab.py band-pass --slopes 15 21 87 --q 0.1 0.71`. Above 24 dB/oct the test allows 5.5 dB there, not 1 dB. The whole orders read the same way: orders 14 to 16 give 4.08–4.94 dB (`--orders 14 15 16`), the doc's measured 4.9, and orders 2 to 4 give 0.57–0.71 dB (`--orders 2 3 4`), inside 1 dB. There is no difference between the lab and the test. One fractional figure is just over: 21 dB/oct at Q 0.71 reads 1.02 dB quieter up to 0.45 × Nyquist (`--slopes 21`), against 1.0. The Engine port (#18) should check it on the test's grid.
- **High Cut rounding at Q 40:** it is in the design's arithmetic, not the evaluation, and the Engine has it today at whole orders. Evaluating the same coefficients in 60-digit arithmetic gives the same response. Designing them in 60 digits gives the target. `match_section` reads the poles' squared magnitude at the match point in the phi form, d0 p0 + d1 p1 + d2 p2. With poles near z = 1 that sum cancels. The analog magnitude at a resonance (about Q²) multiplies its rounding, and p2 (about w²) divides it, which swamps the numerator's p2. That p2 shrinks as w⁴. `_factor` then clamps a negative square, and the level jumps by many dB. `engine/src/MatchedDesign.cpp` uses the same form in double. `filterlab.py matched-rounding` gives High Cut at whole orders 1 to 16, Q 40, Frequency 10 to 40 Hz, against its target:

  | | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz | Jump between Slopes 10⁻⁹ apart (Q 40, 20 Hz, 96 kHz) |
  |---|---|---|---|---|---|
  | Phi form (the Engine's) | 1.5 × 10⁻⁴ dB | 1.2 × 10⁻⁴ dB | 13 dB | 42 dB | 3.2 dB |
  | Read from the poles | 1.5 × 10⁻⁴ dB | 1.2 × 10⁻⁴ dB | 3.1 × 10⁻⁵ dB | 7.8 × 10⁻⁶ dB | 10⁻⁷ dB |

  A scratch program against the Engine's library (`bandResponseDb`) agrees. High Cut at Q 40 reads +13.4 dB at 10 Hz at 96 kHz (13.2 Hz, 18 dB/oct) and +41.4 dB at 192 kHz (11.1 Hz), with 15 and 91 of 656 settings over 0.5 dB. Band Pass at Q 40 jumps 7.7 dB between Frequencies 10⁻⁹ apart at 192 kHz. The response test misses it because its grid starts at 20 Hz and stops at 96 kHz. Reading |1 − p e^(−jw)|² = (1 − r)² + 4r sin²((w − angle)/2) from the poles removes it (`match_section_from_poles`). That fix belongs in the Engine and is outside #196, so the lab keeps the Engine's form.

One Shelf figure stays just over the share limit, and the fractional design doesn't cause it. (The High Shelf at Q 40 up to 0.73 × Nyquist, 16.1% at #161, is 13.2% now that the continuous design uses #195's match point.)

- **High Shelf at Q 0.71, up to 0.91 × Nyquist: 46.1%** of the span alone, which is 1.38 dB on a 3 dB span. The test allows 45% plus 0.6 dB there, so 1.95 dB. Whole order 4 reaches 45.4% at the band's edge.

## Draft for `docs/dsp/filter-design.md`, "Slopes between whole orders"

> - **Slopes between whole orders:** a Slope between whole orders N and N + 1 (t of the way) is order N + 1's cascade morphing from order N's:
>   - Every second-order section both orders share moves its damping (1/Q) linearly with t from order N's value to order N + 1's.
>   - The section order N lacks grows in: a first-order pole coming in from infinity (`ts + 1`) when N is even; when N is odd, order N's real pole gains a second one from infinity (`ts² + ds + 1`), and the two become a pair.
>   - The cascade is rescaled in frequency so Frequency stays at −3 dB (Shelves: at half the Gain in dB), measured at Q 0.71; below 6 dB/oct the pole just comes in from infinity, so a Cut at 0 dB/oct still passes the signal unchanged.
>   - A Shelf's zeros and poles are each prototype pole times and over rz^w, w rising from 0 to 1 for the root coming in from infinity; a High Shelf is the Low Shelf mirrored.
>   - Band Pass and Notch transform the same prototype; a real prototype pole gives one section, and a Band Pass pair's two sections carry the gain |p|²/Q².
>   - Slope sets the steepness near Frequency; a Cut's far stopband falls at the next whole order. The curve may bump against its direction by up to 0.44 dB.
>   - Whole orders are exactly Butterworth. Try it with `filterlab.py <shape> --slopes 15 21 87`.
>
> And under "Method":
>
> - **Fractional Slopes:** whole orders keep their designs. Between them:
>   - The corner rescale is three Newton steps in ln(frequency) from the whole-order corner, measured at Q 0.71.
>   - Every High Cut section is one matched biquad, real poles or a pair, as at whole orders. The partial section has exact poles. While they are real, its match point aims at the analog magnitude times two first-order sections' error there, weighted by 1 − c, with c = 1 − (1 − near/far)²(1 − t).
>   - A Shelf section's resonant split lowers its zeros' Q to the Q the same section has at Q 2, matched at the existing split's match point. The partial section's split is weighted by t³ (and by c while its roots are real), and while real it is matched like High Cut's.
>   - The steep Band Pass design comes in linearly from 24 to 30 dB/oct.
>   - On Band Pass and Notch, a Slope change across the point where the partial section's poles meet crossfades, as does one from 0 dB/oct. Every other Slope change glides.
