# Decramped filter designs instead of oversampling for Zero Latency

In Zero Latency mode, Bands must match their analog response all the way up to Nyquist, so a high Bell or air Shelf at 44.1/48 kHz sounds like the curve the display draws instead of cramping near 20 kHz. We get this from decramped (matched-response) filter designs rather than oversampling, because oversampling adds latency and CPU cost and "Zero Latency" must mean zero.

## Consequences

Every Shape needs its own decramped coefficient design rather than textbook bilinear-transform biquads; a contributor "simplifying" to standard RBJ cookbook filters would reintroduce cramping.

## Known limit of a single decramped biquad

A biquad's response is flat at Nyquist, while a high or wide analog Bell is still sloping there, so one second-order section cannot match the analog curve exactly near Nyquist. The Bell uses matched poles with a numerator that is exact at DC, at its Frequency and at Nyquist. The Engine test bounds its error as a share of |Gain|, by how close the Bell sits to Nyquist: 10% up to 0.45 × Nyquist (about 10 kHz at 44.1 kHz), 20% up to 0.73 × Nyquist (about 16 kHz) and 35% up to 0.91 × Nyquist (about 20 kHz), at 44.1, 48 and 96 kHz. Beyond that, the Bell is only required to stay stable. RBJ bilinear Bells miss by 85–100% on the same grid. Tightening this (a correction section near Nyquist, or an optimised fit) is future work.

## Shelves and tilts

- **Shelves:** Low Shelf, High Shelf and Tilt Shelf are Butterworth shelves of order Slope / 6, in whole steps of 6 dB/oct up to 96 dB/oct. Frequency is where a shelf reaches half its Gain in dB.
  - Q sets the corner resonance. Each second-order section's Q is the Butterworth one scaled by (Q / 0.71)^(1 / sections), so Q 0.71 is a plain Butterworth shelf and a resonant 96 dB/oct shelf rings about as much as a 12 dB/oct one. A 6 dB/oct shelf has no second-order section, so Q has no effect on it.
  - Tilt Shelf is the High Shelf moved down by half its Gain, so it passes 0 dB at Frequency.
- **Decramping:** every section is matched the same way as the Bell, in the direction whose poles sit at or below Frequency, and inverted for the other direction: Low Shelves as boosts, High Shelves as cuts.
- **Flat Tilt:** a straight line of Gain / 10 dB per octave through 0 dB at Frequency. Gain is the tilt across the 10 octaves from 20 Hz to 20 kHz. It is built from 13 first-order shelves, one per octave from 5 Hz to 40 kHz, and stays within 3% of |Gain| of the line.
- **Test tolerances for shelves:** errors are bounded as a share of the curve's own span in dB.
  - 12% up to 0.73 × Nyquist.
  - 45% above that, plus 0.6 dB.
  - 75% for resonant shelves (Q above 2). A single matched biquad cannot follow a section whose zeros and poles are both sharp.
- **Future work:** fractional Slopes, and accuracy for resonant shelves. Splitting a resonant section into a sharp-pole and a sharp-zero biquad brought resonant shelves below 0.45 × Nyquist within 9% in a prototype.
