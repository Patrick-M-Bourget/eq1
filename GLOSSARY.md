# eq1

A real-time equalizer plugin for musicians and producers, modeled on the features and workflow of FabFilter Pro-Q 4. Terms follow Pro-Q's vocabulary so users meet the words they already know.

## Bands

**Band**:
One independently controlled filter in the EQ, defined by its Shape, Frequency, Gain, Q and Slope; a Band Slot that is in use.
_Avoid_: Node, point, filter (when meaning the whole band)

**Band Slot**:
One of the 24 fixed positions a Band can occupy. Deleting a Band frees its slot but keeps the slot's settings.
_Avoid_: Band index, channel

**Shape**:
The kind of filter a Band applies, such as Bell, Notch, Low Shelf, High Shelf, Low Cut, High Cut, Band Pass, Tilt Shelf, Flat Tilt or All Pass.
_Avoid_: Type, filter type, mode

**Frequency**:
The center or corner frequency of a Band, in Hz.
_Avoid_: Freq, cutoff (except informally for Cut shapes)

**Gain**:
The size of a Band's boost or cut, in dB.
_Avoid_: Level, amount, boost

**Q**:
How sharply a Band's curve bends around its Frequency: its width on a Bell, its resonance at the corner of a Shelf or Cut.
_Avoid_: Bandwidth, width, resonance (as the control's name)

**Slope**:
How steeply a Band's curve rolls off beyond its Frequency, in dB per octave.
_Avoid_: Order, steepness, poles

**Brickwall**:
The steepest Slope, available only on Low Cut and High Cut, removing everything beyond Frequency.
_Avoid_: Infinite slope, wall

**Stereo Placement**:
Which part of the stereo signal a Band processes: Stereo, Left, Right, Mid or Side.
_Avoid_: Channel mode, M/S mode

**Bypass**:
Temporarily disabling a single Band while keeping its settings.
_Avoid_: Mute, disable, off

**Solo**:
Auditioning only the region of the spectrum a Band affects.
_Avoid_: Listen, audition (for Bands)

## Dynamics

**Dynamic Band**:
A Band whose Gain moves with the level of its detection signal instead of staying fixed.
_Avoid_: Dynamic filter, compressor band

**Dynamic Range**:
The signed amount, in dB, by which a Dynamic Band's Gain can move away from its static Gain.
_Avoid_: Depth, range, ratio

**Threshold**:
The detection level above which a Dynamic Band starts moving its Gain; it can be set to Auto.
_Avoid_: Trigger level

**Attack**:
How quickly a Dynamic Band's Gain moves once the detection signal exceeds the Threshold.

**Release**:
How quickly a Dynamic Band's Gain returns once the detection signal falls back below the Threshold.

**Sidechain**:
The plugin's separate audio input, used as an external signal for detection rather than for processing.
_Avoid_: Key input, SC

**Detection Source**:
The signal a Dynamic Band listens to: Internal (the main input) or External (the Sidechain).
_Avoid_: Trigger, key

**Detection Range**:
The part of the spectrum a Dynamic Band's detector listens to: Band (follows the Band's own Frequency and Q) or Free (a user-set low and high limit).
_Avoid_: Sidechain filter, trigger range, key filter

## Whole-plugin

**Processing Mode**:
How the whole EQ trades latency against phase behavior: Zero Latency, Natural Phase or Linear Phase.
_Avoid_: Phase mode, quality

**Analyzer**:
The real-time spectrum display behind the EQ curve, showing the signal before and after processing.
_Avoid_: Spectrum, FFT display, meter

**Spectrum Grab**:
Grabbing a peak in the Analyzer to create or adjust a Band at that spot.
_Avoid_: Peak grab, click-to-EQ

**Preset**:
A saved, named set of all plugin settings: Factory (shipped with eq1) or User (saved by the user).
_Avoid_: Patch, program, snapshot

**A/B Compare**:
Switching between two independent sets of settings in one plugin instance to compare them.
_Avoid_: Snapshot, compare slots

**Auto Gain**:
Automatic compensation of output level, estimated from the EQ settings rather than measured, so that EQ changes are heard without a loudness bias.
_Avoid_: Gain compensation, make-up gain

**Gain Scale**:
A single control that scales every Band's Gain and Dynamic Range at once.
_Avoid_: Depth, master gain, amount

**Host Automation**:
Parameter changes recorded and played back by the DAW hosting the plugin.
_Avoid_: Automation (unqualified)
