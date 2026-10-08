# Frozen Shape and Slope parameter layout

Hosts store automation as normalised 0–1 values, so the meaning of every value of `band<n>_shape` and `band<n>_slope` is frozen at the first release. Changing it would silently select different Shapes and Slopes in saved sessions. We froze them before release, matching Pro-Q 4 so settings translate one to one:

- **`band<n>_shape`** lists the Shapes in Pro-Q 4's order: Bell, Low Shelf, Low Cut, High Shelf, High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt, All Pass. It has no reserved entries. A new Shape would need a new parameter and a state migration.
- **`band<n>_slope`** is one continuous linear range, 0–96 dB/oct, shared by every Shape. The Engine raises it to each Shape's minimum: 0 for Cuts and Band Pass, 12 for Bell and Notch, 6 for the rest. The stored value is kept, so switching Shape and back restores it. Fractional Slopes (#18) can be added later without moving any values, because a step change doesn't alter a linear range's normalised mapping.
- **Brickwall** is a separate switch, `band<n>_brickwall`. It only works on Low Cut and High Cut, where it overrides Slope. In the domain and the UI it is still the steepest Slope.

## Considered Options

- **Adding the new Shapes after the existing five.** We rejected it because the host's parameter list and Pro-Q's order would no longer match, and nothing released yet depended on the old order.
- **Brickwall as the top of the Slope range.** We rejected it because sweeping Slope automation on a Cut would jump to a different filter at the end. Every other Shape would also have a stretch at the top of the range that does nothing.
- **A Slope parameter per Shape.** We rejected it because it multiplies the host parameter count across 24 Band Slots, for no audible gain.
