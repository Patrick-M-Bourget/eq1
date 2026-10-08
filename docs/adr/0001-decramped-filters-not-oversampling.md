# Decramped filter designs instead of oversampling for Zero Latency

In Zero Latency mode, Bands must match their analog response all the way up to Nyquist, so a high Bell or air Shelf at 44.1/48 kHz sounds like the curve the display draws instead of cramping near 20 kHz. We get this from decramped (matched-response) filter designs rather than oversampling, because oversampling adds latency and CPU cost and "Zero Latency" must mean zero.

## Consequences

Every Shape needs its own decramped coefficient design rather than textbook bilinear-transform biquads; a contributor "simplifying" to standard RBJ cookbook filters would reintroduce cramping.

## Known limit of a single decramped biquad

A biquad's response is flat at Nyquist, while a high or wide analog Bell is still sloping there, so one second-order section cannot match the analog curve exactly near Nyquist. The Bell uses matched poles with a numerator that is exact at DC, at its Frequency and at Nyquist. The Engine test bounds its error as a share of |Gain|, by how close the Bell sits to Nyquist: 10% up to 0.45 × Nyquist (about 10 kHz at 44.1 kHz), 20% up to 0.73 × Nyquist (about 16 kHz) and 35% up to 0.91 × Nyquist (about 20 kHz), at 44.1, 48 and 96 kHz. Beyond that, the Bell is only required to stay stable. RBJ bilinear Bells miss by 85–100% on the same grid. Tightening this (a correction section near Nyquist, or an optimised fit) is future work.
