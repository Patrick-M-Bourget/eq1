# Dynamic Bands reach their full range at a fixed overshoot, and Auto Threshold follows the material's spread

A Dynamic Band in Auto barely moved on ordinary material: about 1 dB on a vocal swinging ±6 dB in its region, whatever its Dynamic Range. Two rules caused it. The gain computer reached the full Dynamic Range only 2 × |Dynamic Range| + 3 dB above Threshold, so a larger range needed more overshoot and moved less. Auto Threshold sat 4 dB above the region's mean level over 2 s, which material with an ordinary spread barely crosses, and which learned any swell lasting a second.

We changed both, before the first release:

- **Gain computer:** the full Dynamic Range arrives at a fixed overshoot above Threshold (plus the soft knee), whatever the size of the range. A larger Dynamic Range means deeper movement, not slower. This applies to a set Threshold and to Auto alike.
- **Auto Threshold:** the region's mean level plus a fraction of the level's spread (its standard deviation), so a Band moves on what stands out of the material, whether that material swings 3 dB or 20 dB. It rises slowly and falls quickly, so a sustained swell still moves the Band instead of being learned. Until it has heard about 250 ms of the region, the Band holds at its Gain.

The numbers (the overshoot, the spread fraction, the rise and fall times, the hold) are tuned against the acceptance tests and recorded in `docs/dsp/filter-design.md` ("Dynamics"), not here.

## Considered Options

- **A user offset for Auto Threshold ("Sensitivity").** Rejected: a new per-Band control the spec doesn't have, and an Auto that follows the material's spread doesn't need it.
- **Only lowering Auto Threshold's margin.** Rejected: how far a fixed margin sits above the material's peaks depends on how dynamic the material is.
- **A floor tracker with a fixed offset** (a slow detector of the region's quiet level, Threshold a set amount above it). Rejected for the same reason as a fixed margin, and it needs the offset control.
- **A second, fixed-timing detector for transients.** Rejected: it would override the Band's Attack and Release.

## Consequences

No parameter changes, so the state version stays. Sessions and Presets saved before the change sound more dynamic: their Dynamic Bands move further at the same settings. Nothing has shipped, so there is no migration. Factory Presets are tuned by ear after this change.
