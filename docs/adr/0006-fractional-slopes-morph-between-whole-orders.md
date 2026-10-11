# Fractional Slopes morph the cascade between neighbouring whole orders

**Status:** Accepted (#18): the partial cascade; beyond about an octave the stopband falls at the next whole order. Jumps between nearby Slopes and the Shelf tolerance gaps are explored in #161 before the Engine work.

`band<n>_slope` is already continuous (ADR 0003), but the Engine rounds it to the nearest whole order. Pro-Q 4's Slope is continuous (for example 3.5 dB/oct). This ADR proposes the analog target for a Slope between whole orders. Bell's Slope is #19.

It covers the Shapes that round today: Low Shelf, High Shelf, Tilt Shelf, Low Cut, High Cut, Band Pass and Notch. Write the order as ν = Slope / 6 (Slope / 12 for Notch), with N = ⌊ν⌋ and t = ν − N.

## Proposal: the morphing cascade is the target

The cascade at order ν is built from order N + 1's sections, as follows:

- **Shared second-order sections:** each one that orders N and N + 1 both have moves its damping (1/Q) linearly with t, from order N's Butterworth value to order N + 1's. Q scaling is applied at both ends first.
- **The partial section:** the section that order N lacks grows in.
  - N even: a first-order pole comes in from infinity, `ts + 1`.
  - N odd: order N's real pole gains a second pole from infinity, `ts² + ds + 1`. The two real poles meet and become a pair before t reaches 1.
- **Corner:** the cascade is rescaled in frequency so Frequency stays at −3 dB, measured at Q 0.71. Below 6 dB/oct nothing is rescaled: the pole simply comes in from infinity.
- **Shelves:** each prototype pole r gives a zero at r·rz^w and a pole at r/rz^w, with rz^(2Σw) = Gain. Here w is 1 for a whole root. For the root coming in from infinity, w rises from 0 to 1 by the point where it meets order N's real pole. So the root's step comes in gradually. The shelf is then rescaled so Frequency is at half the Gain in dB. A High Shelf is that Low Shelf mirrored (s → 1/s); unlike Butterworth's, it isn't the Low Shelf of −Gain moved up by Gain.
- **Band Pass and Notch:** the same prototype goes through their existing transforms. A real prototype pole gives one band-pass or notch section; a pair gives two.

At t = 0 the cascade is exactly order N's Butterworth cascade, at every Q: the lab's whole-order designs match the existing ones within 10⁻¹² dB. Every coefficient moves continuously with Slope.

Why this one:

- **Accuracy:** it is the only candidate the matched design meets with whole-order accuracy in every region and at every Q. It is its own target, so all that is left is decramping error.
- **Cost:** it needs no sections beyond the next whole order's. At 96 dB/oct nothing changes: Cuts take 8 biquads, Shelves 16, Band Pass 16 and Notch 8.
- **Q:** it extends to Q the same way whole orders do.

What it costs:

- **Far stopband:** past roughly an octave beyond Frequency (where the partial pole sits), a Cut falls at the next whole order, 6⌈ν⌉ dB/oct, not at Slope. So Slope sets the steepness near Frequency.
- **Small bumps and reversals:** a curve can bump against its own direction by up to 0.44 dB on Cuts and Band Pass, 0.34 dB on Notch and 0.22 dB on Shelves. As Slope rises, a Shelf's steepness can briefly ease by up to 0.59 dB, and a Band Pass's by up to 0.91 dB.

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
- **Accuracy work before the Engine port** (lab numbers at 15, 21 and 87 dB/oct, against whole orders 2, 3, 4, 14, 15 and 16):
  - Resonant Low Shelves reach 17.6% (Q 3) and 26.3% (Q 10) of the span up to 0.73 × Nyquist, against 9.8% and 10.3% at whole orders, and over the 15% limit. The resonant split isn't tuned for the partial section.
  - High Shelves reach 46.3% (Q 0.71) and 50.4% (Q 3) up to 0.91 × Nyquist, over the 45% limit.
- **Design seams** in the lab's matched design, where the response jumps between two Slopes 0.01 dB/oct apart:
  - High Cut: 1.15 dB where the partial section's poles meet. The lab designs two real poles as first-order sections and a pair as one matched biquad.
  - Band Pass: 7.7 dB at the same point.
  - Band Pass at 24 dB/oct, where the steep design switches on.
  - Each needs a design that is continuous there, or the crossfade the Engine already uses for Slope changes. Crossfading keeps a Slope sweep click-free today, at the price of stepping.
- **Steep change after each whole order:** just past a whole order, the analog curve itself changes quickly, because the incoming pole sweeps through the audio band. For example, a Low Cut at 2 kHz moves up to 0.74 dB per 0.01 dB/oct. Gliding Slope is still continuous.
- **The corner rescale** is a 1-D search per design in the lab: over ν for Cuts, and over ν and Gain for Shelves. The Engine would read it from a table or run a few Newton steps.
- **No parameter changes.** Sessions saved with a rounded Slope sound different once fractional Slopes ship. That is unreleased behaviour, so there is no migration.

## Draft for `docs/dsp/filter-design.md`, "Slopes between whole orders"

> - **Slopes between whole orders:** a Slope between whole orders N and N + 1 (t of the way) is order N + 1's cascade morphing from order N's:
>   - Every second-order section both orders share moves its damping (1/Q) linearly with t from order N's value to order N + 1's.
>   - The section order N lacks grows in: a first-order pole coming in from infinity (`ts + 1`) when N is even; when N is odd, order N's real pole gains a second one from infinity (`ts² + ds + 1`), and the two become a pair.
>   - The cascade is rescaled in frequency so Frequency stays at −3 dB (Shelves: at half the Gain in dB), measured at Q 0.71; below 6 dB/oct the pole just comes in from infinity, so a Cut at 0 dB/oct still passes the signal unchanged.
>   - A Shelf's zeros and poles are each prototype pole times and over rz^w, w rising from 0 to 1 for the root coming in from infinity; a High Shelf is the Low Shelf mirrored.
>   - Band Pass and Notch transform the same prototype; a real prototype pole gives one section.
>   - Slope sets the steepness near Frequency; a Cut's far stopband falls at the next whole order. The curve may bump against its direction by up to 0.44 dB.
>   - Whole orders are exactly Butterworth. Try it with `filterlab.py <shape> --slopes 15 21 87`.
