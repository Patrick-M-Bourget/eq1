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

## Test tolerances

The Engine tests (`tests/engine/BellResponseTest.cpp`, `tests/engine/ShapeResponseTest.cpp`) bound the error by where Frequency sits relative to Nyquist, at 44.1, 48 and 96 kHz. Above 0.91 × Nyquist a Shape is only required to stay stable.

| Shape | Up to 0.45 × Nyquist | Up to 0.73 × Nyquist | Up to 0.91 × Nyquist |
| --- | --- | --- | --- |
| Bell (share of \|Gain\|) | 10% | 20% | 35% |
| Shelves, Q ≤ 2 (share of the curve's span in dB) | 12% | 12% | 45% + 0.6 dB |
| Shelves, Q > 2 (share of the curve's span in dB) | 75% | 75% | 75% + 0.6 dB |

- **Flat Tilt:** within 0.05 dB + 3% of |Gain| of the line, from 20 Hz to 30 kHz or 0.9 × Nyquist, whichever is lower.
- **Resonant shelves (Q above 2):** the bound is loose because a single matched biquad cannot follow a section whose zeros and poles are both sharp.

## Open work

- **#17, resonant shelf accuracy:** splitting a resonant section into a sharp-pole biquad and a sharp-zero biquad brought shelves below 0.45 × Nyquist within 9% in the #4 prototype.
- **#18, fractional Slopes.**
- **#19, Bell Slope:** every Bell is 12 dB/oct for now, while Pro-Q 4 goes up to 96. A known difference (ADR 0003).
- **The Bell near Nyquist:** a correction section, or an optimised fit.
