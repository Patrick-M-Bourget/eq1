# Decramped filter designs instead of oversampling for Zero Latency

In Zero Latency mode, Bands must match their analog response all the way up to Nyquist, so a high Bell or air Shelf at 44.1/48 kHz sounds like the curve the display draws instead of cramping near 20 kHz. We get this from decramped (matched-response) filter designs rather than oversampling, because oversampling adds latency and CPU cost and "Zero Latency" must mean zero.

## Consequences

Every Shape needs its own decramped coefficient design rather than textbook bilinear-transform biquads; a contributor "simplifying" to standard RBJ cookbook filters would reintroduce cramping.

The rule is about magnitude, so the All Pass is exempt: its magnitude is flat by construction, and a bilinear all-pass prewarped at Frequency gets the phase exactly right there, which matched poles don't.

Matching near Nyquist has limits: a biquad's response is flat at Nyquist, while a high or wide analog curve still slopes there. Each Shape's analog target, its design and the accuracy its tests enforce are in `docs/dsp/filter-design.md`.
