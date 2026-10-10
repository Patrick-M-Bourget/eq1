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
The size of a Band's boost or cut, in dB, as the user sets it.
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

**Split**:
Turning one Stereo Band into two Bands, one Left and one Right, that keep every other setting.
_Avoid_: Duplicate, separate

**Invert Gain**:
Flipping the sign of a Band's Gain and Dynamic Range, so a boost becomes the same cut.
_Avoid_: Flip, Phase Invert (for a Band)

## Dynamics

**Dynamic Band**:
A Band whose Shape allows dynamics and whose Dynamic Range is not zero, so its Live Gain moves with the level of its detection signal.
_Avoid_: Dynamic filter, compressor band

**Dynamic Range**:
The signed amount, in dB, by which a Dynamic Band's Live Gain can move away from its Gain.
_Avoid_: Depth, range, ratio

**Live Gain**:
The Gain a Dynamic Band is applying at this moment, between its Gain and its Gain plus Dynamic Range.
_Avoid_: Current gain, dynamic gain, Gain (when meaning the moving value)

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

**Dynamics Bypass**:
Temporarily disabling a Dynamic Band's movement while keeping its settings; its Live Gain stays at its Gain.
_Avoid_: Bypass (unqualified) for dynamics

**Detection Audition**:
Listening to the signal a Dynamic Band's detector hears instead of the plugin's output.
_Avoid_: Sidechain solo, listen, Solo (for detection)

**Detection Level**:
The level of a Band's detection signal, as its detector compares it with the Threshold, in dB where a full-scale sine reads 0.
_Avoid_: Input level, sidechain level

**Metered Band**:
The one Band whose Detection Level is shown, whatever its dynamics state: a Band in use, not Bypassed, whose Shape has dynamics.
_Avoid_: Monitored Band, Band meter

**Clear Dynamics**:
Putting every dynamics setting of a Band back to its default, so it stops being a Dynamic Band.
_Avoid_: Reset (unqualified), remove dynamics

## Whole-plugin

**Processing Mode**:
How the whole EQ trades latency against phase behavior: Zero Latency, Natural Phase or Linear Phase.
_Avoid_: Phase mode, quality

**Analyzer**:
The real-time spectrum display behind the EQ curve, showing the signal before and after processing, and the Sidechain.
_Avoid_: Spectrum, FFT display, meter

**Analyzer Tilt**:
A display-only slope applied to the Analyzer's spectra so that typical music looks level; it doesn't change the sound.
_Avoid_: Tilt (unqualified) for the Analyzer

**Spectrum Grab**:
Grabbing a peak in the Analyzer to create or adjust a Band at that spot.
_Avoid_: Peak grab, click-to-EQ

**Peak Hold**:
A faint line on the Analyzer that holds the highest level each frequency reached, then falls back slowly.
_Avoid_: Freeze, max hold

**Display Range**:
How many dB above and below 0 dB the EQ display shows Gain over; it changes the view, never the sound.
_Avoid_: Zoom, range (unqualified), scale

**Preset**:
A saved, named set of every setting that affects the sound: Factory (shipped with eq1) or User (saved by the user).
_Avoid_: Patch, program, snapshot

**Loaded Preset**:
The Preset last loaded onto, or saved from, an A/B Compare side, shown by name. Each side has its own, or none until a Preset is loaded or saved on it.
_Avoid_: Current preset, active preset, selected preset

**Modified**:
An A/B Compare side whose settings differ from its Loaded Preset's; returning to the Preset's settings, by edits or undo, makes it unmodified again.
_Avoid_: Dirty, edited, changed (as a state)

**A/B Compare**:
Switching between two independent sets of the settings a Preset holds, in one plugin instance, to compare them.
_Avoid_: Snapshot, compare slots

**Auto Gain**:
Automatic compensation of output level, estimated from the EQ settings rather than measured, so that EQ changes are heard without a loudness bias.
_Avoid_: Gain compensation, make-up gain

**Global Bypass**:
eq1's own click-free switch that passes the input through unprocessed, separate from the host's bypass.
_Avoid_: Bypass (unqualified) for the whole plugin

**Output Gain**:
The gain applied to the whole plugin's output, after every Band, in dB.
_Avoid_: Master gain, Gain (unqualified)

**Output Pan**:
The balance of the whole plugin's output between the two sides of its Pan Mode; the centre leaves both alone. Unavailable on mono.
_Avoid_: Balance, Pan (unqualified) when a Band's Stereo Placement could be meant

**Pan Mode**:
What Output Pan balances: Left against Right (L/R), or Mid against Side (M/S).
_Avoid_: Stereo mode, Channel mode

**Phase Invert**:
Flipping the polarity of the whole plugin's output.
_Avoid_: Polarity flip, phase flip, invert

**Gain Scale**:
A single control that scales every Band's Gain and Dynamic Range at once.
_Avoid_: Depth, master gain, amount

**Host Automation**:
Parameter changes recorded and played back by the DAW hosting the plugin.
_Avoid_: Automation (unqualified)

**Output Level**:
The sample peak and RMS of what eq1 sends on, per channel, in dBFS; the input during Global Bypass.
_Avoid_: Level (unqualified), loudness, output gain

**Clip Light**:
A per-channel indicator that lights when the Output Level peaks above 0 dBFS and stays lit until clicked.
_Avoid_: Clip indicator, over light

**Output Meter**:
The display of the Output Level per channel, with a Clip Light; separate from the Analyzer.
_Avoid_: Meter (unqualified), level meter, VU

## Licensing

**Tier**:
What this machine is entitled to use: the Free Tier, a Trial or the Pro Tier. It belongs to the machine, never to a session or Preset.
_Avoid_: Edition, plan, license level

**Free Tier**:
The Tier that needs no License Key: every feature, up to the Band Cap.
_Avoid_: Free (unqualified, which is a Detection Range), Lite, demo

**Trial**:
The Tier that allows what the Pro Tier does, for 14 days from when the user starts it, once per machine; the machine is on the Free Tier again when it ends.
_Avoid_: Demo, evaluation

**Pro Tier**:
The paid Tier, unlocked by a License Key: all 24 Band Slots.
_Avoid_: Full version, paid version, Pro (unqualified)

**Band Cap**:
The number of Bands that process audio under the Free Tier: 6. Bands beyond it are kept but bypassed.
_Avoid_: Band limit, band count

**License Key**:
A signed proof of purchase that names its owner and the major version it unlocks.
_Avoid_: Serial, activation code, license file

**Heartbeat**:
The anonymous report, at most once a day, that an eq1 install is in use, with its Tier; the reply tells eq1 the latest version.
_Avoid_: Ping, analytics, phone-home

**Update Notice**:
A dismissible sign in the editor that a newer version of eq1 is available.
_Avoid_: Update alert, update prompt
