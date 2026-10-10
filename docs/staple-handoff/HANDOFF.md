# Staple EQ — UI handoff for JUCE

This is the build brief for the Staple EQ plugin UI. The reference is the interactive prototype on the
Claude Design canvas (`Main.dc.html` = plugin window, `DesignSystem.dc.html` = design system,
`tokens.css` = tokens). Treat the prototype as the **visual and behavioural source of truth**, not as
code to port: it is one large web component with mock signals standing in for DSP.

> **Scope: UI only.** The DSP (filters, dynamics, Sidechain, Processing Modes, Auto Gain, Analyzer FFT, metering) is
> being built separately and already exists or is in progress. Your job is the editor: bind every control to the
> **existing** processor parameters and read the processor's live data for display. Do not implement or change
> audio processing. See §2a for the integration contract.

Files in this folder:

| File | What it is |
|---|---|
| `HANDOFF.md` | This brief |
| `tokens.json` | All design tokens (colour, type, radius, motion, band palette) |
| `plugin/staple/Tokens.h` (in the build, moved from this folder) | The same tokens as `juce::Colour` / float constants, ready to include |
| `logo.png` | Current logo (183 × 114 source — request an SVG before release) |
| `prototype/` | Snapshot of the prototype source (`Main.dc.html`, `DesignSystem.dc.html`, `tokens.css`) for reading exact values, icon paths and behaviour. Reference only — do not port. |

---

## 0. Terminology

The build's **`GLOSSARY.md` is authoritative** for every user-facing word (labels, tooltips, menu items,
accessibility names) and for names in code where practical. This brief and the prototype use its terms:
Band, Band Slot, Shape, Frequency, Gain, Q, Slope, Brickwall, Stereo Placement, Bypass, Solo, Dynamic Band,
Dynamic Range, Live Gain, Threshold, Attack, Release, Sidechain, Detection Source, Detection Range, Dynamics Bypass,
Detection Audition, Processing Mode, Analyzer, Analyzer Tilt, Spectrum Grab, Preset, Loaded Preset, Modified,
A/B Compare, Auto Gain, Global Bypass, Output Gain, Output Pan, Pan Mode, Phase Invert, Gain Scale, Host Automation.
Respect its *Avoid* lists (e.g. never "node" for a Band, "type" for Shape, "SC", "listen", "disable", "polarity").

UI-only names that the glossary doesn't cover: **Band handle** (the draggable dot on the curve), **Band panel**,
**hover card**, **Edge selector**, **Dynamic Range ring** (outer ring of the Gain knob), **Detection Range bar**
(Free mode), **Display range** (±3…±30 dB view zoom), **UI scale**. *Staple* is the product/brand name shown in the
header; *eq1* is the build's codename.

## 1. Ground rules

1. **Colour belongs to the Bands.** Everything else is neutral. Fixed semantic colours never take a Band colour:
   red = Bypass and Dynamic Range, yellow = main curve and Live Gain, teal = Sidechain spectrum.
2. **No outlines on hover.** Hover lights a control up (icon to `text1`, or brightness ×1.18 on filled controls).
   Pressed = brightness ×1.3. The only boxed icon state is **Off** (red icon on red tint).
3. **Nothing reflows.** The Band panel is fixed width (it only grows when the dynamics section is open);
   disabled controls dim to 35 % instead of disappearing.
4. **One font:** Manrope (bundle it as BinaryData; do not rely on system fonts). All numbers use tabular figures
   (`juce::FontOptions` with the `tnum` feature, or a tabular cut) so values never jitter.
5. **Every value comes from `plugin/staple/Tokens.h`.** No hard-coded colours in components.

## 2. Suggested JUCE architecture

```
PluginEditor (1200 × 760 base, see §9 scaling)
├── HeaderBar            logo · preset ‹ name › · undo/redo · A/B · Copy
├── DisplayComponent     grid, Analyzer, curves, Band handles, ghost, overlays (OpenGL or cached Path rendering)
│   ├── BandHandle ×24   lightweight, painted by DisplayComponent (not child Components) for perf
│   ├── HoverCard        quick-controls card (child Component, shown on Band handle hover)
│   ├── BandPanel        bell-shaped floating panel for the selected band
│   │   ├── EdgeSelector (Shape)     ShapeMenu
│   │   ├── Knob ×3 (Freq, Gain+DynRing, Q) + KnobTooltip
│   │   ├── DynamicsSection (collapsible)
│   │   └── EdgeSelector (Placement) PlacementMenu
│   ├── ListenRangeBar   Free-mode detection filter bar
│   └── RangeSelector    ±3/6/12/18/30 dB
├── OutputMeter          (hideable)
├── FooterBar            Phase · Analyzer▾ · gain scale % · output dB▾
├── AnalyzerPopover / OutputPopover / PresetBrowser (modal) / ContextMenu
└── LookAndFeel_Staple   all drawing for knobs, buttons, menus, popovers
```

- **Parameters:** bind to the processor's existing `AudioProcessorValueTreeState` (see §2a). UI-only state
  (selection, open menus, hover, analyzer display settings, meter visibility, window size, UI scale) lives in a
  separate `ValueTree` saved with the plugin state.
- **Undo:** `juce::UndoManager` on the APVTS. Group changes within **600 ms** into one transaction;
  keep **100** levels. Cmd/Ctrl+Z undo, Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y redo.
- **Repaint:** analyzer + meters + gain-reduction ring at 30–60 Hz via a single `VBlankAttachment`/Timer on the
  editor; everything else repaints on parameter change only.
- **Curve maths:** the prototype uses RBJ biquads (`coeffs()` / `bandDb()`) evaluated every 4 px on a log axis
  20 Hz–20 kHz as a stand-in. In JUCE, get magnitude responses **from the DSP's own filter design** (e.g. a
  `getMagnitudeForFrequency` per Band) so curve and audio match exactly.

## 2a. Integration contract (UI ↔ DSP)

**Parameters**
- Use the processor's parameter IDs exactly as they exist. **Never invent, rename or re-range parameters.**
- §3 lists what the UI expects. Before building, produce a short mapping table *UI control → existing parameter
  ID* and flag anything missing or different (range, choice list, default) for the DSP author to decide.
- Attach with `SliderAttachment` / `ButtonAttachment` / `ComboBoxAttachment`, or `ParameterAttachment` for custom
  controls (knobs, Band handles, ring, fader). Wrap every drag in `beginChangeGesture` / `endChangeGesture` so hosts
  record clean automation and undo groups correctly.
- Band slot activity (which of the 24 slots are in use) comes from the processor; creating/deleting a band just
  enables/disables a slot and sets its parameters.

**Live data the UI reads** (lock-free, polled at UI rate; ask the DSP author for the actual interface)

| UI element | Data needed |
|---|---|
| Analyzer pre / post | FFT magnitude frames (pre-EQ and post-EQ) |
| Sidechain spectrum | FFT of the Sidechain |
| Dynamic Range ring yellow fill, moving curve, Threshold meter | Per-Band Live Gain (dB) and detector level |
| Output meter + clip lights | Output peak (and RMS if available) per channel |
| Footer Output Gain readout with Auto Gain | Current Auto Gain compensation (dB) |
| Processing Mode | Reported latency (show nothing special unless the DSP exposes it) |

If something in this table doesn't exist yet, stub it behind an interface with a clearly marked TODO and keep the
UI rendering sensibly with zeros — do not simulate signals in release code.

## 3. Parameters

These are the controls the UI expects. Match them to the processor's existing IDs (§2a); where the DSP differs,
the DSP wins and the UI adapts its labels/ranges.

### Per Band (24 Band Slots)

| Term (glossary) | Range | Default | Display | Notes |
|---|---|---|---|---|
| **Bypass** | bool | off | power icon | Bypassed Band: see §5.4 |
| **Solo** | bool, one Band at a time | off | headphones in the Band panel top row | Exclusive: soloing a Band un-solos the others |
| **Shape** | Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt, All Pass | Bell | name + icon | Gain dimmed for Low/High Cut, Notch, Band Pass, All Pass; Q dimmed for Flat Tilt |
| **Frequency** | 20 Hz – 20 kHz, log | from creation point | `<1 kHz`: whole Hz; `≥1 kHz`: 2 decimals kHz (1 decimal ≥10 kHz) | |
| **Gain** | −30 … +30 dB | 0 (Bell: from click height) | 1–2 decimals | Displayed × Gain Scale |
| **Q** | 0.1 – 40, log | Bell 1.0, Cuts 0.71 | 2 decimals (3 below 1) | |
| **Slope** | 6, 12, 18, 24, 30, 36, 48, 72, 96 dB/oct, **Brickwall** | 12 | `12 dB/oct` / `Brickwall` | Low/High Cut only; Brickwall only there |
| **Stereo Placement** | Stereo, Left, Right, Mid, Side | Stereo | name + dot colour | |
| **Dynamic Range** | −24 … +24 dB (0 = static) | 0 | `+6.0 dB` | Non-zero on a dynamics-capable Shape makes a **Dynamic Band** |
| **Dynamics Bypass** | bool | off | power icon above Gain | Live Gain stays at Gain |
| **Threshold** | −60 … −1 dB, or **Auto** | Auto | `−24 dB` / `A` | Top of the fader travel = Auto |
| **Attack** | 0.5 – 200 ms, log | 10 ms | ms / s | |
| **Release** | 10 – 2000 ms, log | 120 ms | ms / s | |
| **Detection Source** | Internal, External (Sidechain) | Internal | →| icon, lit = External | **Per Band only**; there is no global Sidechain switch in the UI |
| **Detection Range** | Band, Free | Band | Band / Free | Free reveals the Detection Range bar |
| Detection Range low / high | 20 Hz – 20 kHz, low < high ÷ 1.25 | the Band's own range on first switch | Hz/kHz | Remembered when switching back to Band |
| **Detection Audition** | bool | off | headphones in the dynamics section, Band colour when on | |

Band Slots: 24. Each Band keeps a stable colour slot (0–23) assigned at creation (lowest free). Deleting a Band frees
its Band Slot but the slot keeps its settings (glossary) — the UI just stops showing it.
Live Gain (read from the DSP) drives the yellow fill on the Dynamic Range ring and the moving curve.

### Global

| Term (glossary) | Range | Default | Notes |
|---|---|---|---|
| **Global Bypass** | bool | off | Footer far left, ⌘/Ctrl+B. Full behaviour in §5.12 |
| **Processing Mode** | Zero Latency, Natural Phase, Linear Phase | Zero Latency | Footer |
| **Gain Scale** | 0 – 200 % | 100 % | Scales every Band's Gain **and Dynamic Range** (curve and Band handles move). Double-click = 100 % |
| **Output Gain** | −24 … +24 dB | 0 dB | Output popover knob |
| **Output Pan** | −100 … +100 | 0 | Balances the two sides of the Pan Mode; unavailable on mono (dim the slider) |
| **Pan Mode** | L/R, M/S | L/R | Chip next to the Output Gain knob |
| **Phase Invert** | bool | off | Output popover |
| **Auto Gain** | bool | off | Footer readout shows Output Gain **including** the Auto Gain compensation |
| **A/B Compare** | A, B | A | One text button; Copy copies the active side's settings (and its Loaded Preset) to the other |
| Display range (UI) | ±3, 6, 12, 18, 30 dB | ±18 | UI state; auto-zooms out (§5.9) |

**Loaded Preset / Modified:** each A/B Compare side has its own Loaded Preset (or none until a Preset is loaded or
saved on it — the header then reads "No preset" in `text3`). When a side's settings differ from its Loaded Preset, a
5 px `text3` dot follows the name (accessible name "Modified"); returning to the Preset's settings removes it.

**Analyzer** (UI state): sources Pre / Post / Sidechain, Range 60 / 90 / 120 dB, Resolution low/medium/high,
Speed slow/medium/fast, **Analyzer Tilt** off/3/4.5/6 dB/oct, Peak hold on, Freeze off.

## 4. Visual spec (reference tokens by name — see `plugin/staple/Tokens.h`)

**Window:** `bg0` with three very soft neutral radial highlights (no hue). 14 px outer padding.

**Display (1134 × 612; 1186 when the meter is hidden):**
- Grid: minor lines white 2.5 %, major 6 %, 0 dB line 22 %.
- Edges dissolve: grid, analyzer and curve fills fade to transparent over 18 px top, 84 px bottom,
  36 px left, 56 px right. Band handles, grips and the Band panel are **not** faded.
- Analyzer: smoothed spectrum fill (gradient `#C3CCE6` 20 % → 0) + 1 px line 42 %; peak-hold 1 px line 16 %;
  pre spectrum 1 px solid 20 % (Pre+Post only); sidechain spectrum 1 px solid `anSc` 55 %, smoothed.
- Curves: sum 2 px `curveMain` + 4 px blurred halo `curveMainHalo`; selected Band 1.5 px Band colour + 30 % fill
  + 2 px glow at 30 %; other Bands 1 px at 50 % + 10 % fill (hover: 95 % / 26 %, 220 ms fade).
- Dynamic Range on the curve: 20 % Band-colour wash between the Gain and Gain + Dynamic Range curves, **no outline**.

**Band handles:** flat dots, band colour with a centred 16 % white sheen; 16 px (hover ×1.15); selected 22 px
with a 2 px white ring and a 10 px band-colour glow at 40 %; shadow centred `0 0 5px rgba(0,0,0,.45)`.
Bypassed: same size, colour desaturated (`oklch(0.72 0.045 h)`), no glow.

**Band panel:** bottom-centre, 36 px above the display bottom. Body is a rounded slab with a 22 px **bell** rising
from the centre of the top edge (Gaussian, σ ≈ 14 % of width). Fill `raised` + radial band-colour wash from the top
centre (14 % → 3.5 %), and a 1 px band-colour hairline tracing the top edge (fading to 0 at both ends, 45 % centre).
Corners `r3`. Padding 22 / 26 / 10 / 26.
- Top row: Bypass and Solo (left), Band selector `‹ n ›` + delete ✕ (right), 24 px icon buttons.
- **Edge selectors** (Shape left, Stereo Placement right): 104 × 34, flush with the panel side, rounded only on the
  inner side, base `#141519`, 1 px band-colour hairline brightest at the inner edge fading toward the panel edge,
  plus a 12 % band-colour wash from the inner top corner. Slope text button (90 px) under Shape.
- Knobs: Frequency 50 px, Gain 66 px, Q 50 px; labels on one baseline; no value read-outs.

**Knobs:** face `radial-gradient(#35373D → #27282D → #1C1D21)` from the top, 2 px rim white 22 %
(1.5 px on 30 px knobs), soft drop shadow, **no outer black ring**. Value arc 3 px band colour (2 px on small
knobs), radius = r − max(4.5, d × 0.1), bipolar knobs start at 12 o'clock. **No background track** —
only the active arc is drawn. Arc sweep 270° (−135° … +135°).

**Dynamic Range ring (Gain knob):** 12 px lane at r + 10, flat white 6 %. Dynamic Range: red radial gradient
(`#9C3344` inner → `dynRange` outer) at 85 % (30 % under Dynamics Bypass). Live Gain: yellow radial
gradient (`#C49A34` → `dynLive`) filling from Gain to the current Live Gain. ▲▼ grip (white 80 %) sits at the
range end, rotated to point along the ring.

**Knob tooltip (hover/drag):** `menu` fill, `r2`, `shadow1`, two lines: "Band 4 Gain" (`fs2`, `text3`) /
value (`fs4`, `text1`). Shows *Dynamic Range* when the pointer is over the ring.

**Menus / popovers / cards:** `menu` (opaque), 1 px `line2`, `r3`, `shadow1`, pop-in 180 ms
(translate 3 px + scale 0.985 → 1). Items 28–30 px, hover `fill1`.

## 5. Behaviour

### 5.1 Display
- **Double-click empty space:** add a Band (max 24). Left 4 % of width = Low Cut, right 4 % = High Cut, else Bell at
  the click's Gain. Empty state shows a ghost bell + frequency read-out that follow the pointer (peak kept ≥ 60 px
  from top/bottom so its glow is never clipped).
- **Click inside a Band's filled curve** selects it (including bypassed Bands). Click empty space deselects.
- **Right-click:** context menu (title = Band or "N Bands"): Bypass / Remove Bypass, Invert Gain, Clear Dynamics,
  Shape ▸, Slope ▸, Stereo Placement ▸, Split (Left / Right), Cut, Copy, Paste, Delete, Select All.

### 5.2 Band handles
- Drag: Frequency (x) and Gain (y, inverse of Gain Scale). Hover shows the quick-controls card after a short delay;
  it hides 220 ms after leaving.
- Keyboard (focused Band handle): ←/→ ±1 semitone, ↑/↓ ±0.5 dB, Delete/Backspace removes.

### 5.3 Knobs (all)
- Vertical drag: full range over 200 px; Shift = fine (800 px).
- Double-click knob = reset to default. Double-click the tooltip = type a value (Enter commits, Esc cancels).
- Arrow keys step 1 % (Shift 0.2 %). Gain knob: Alt+arrows adjust the Dynamic Range.
- Gain knob: dragging on the **outer ring** sets the Dynamic Range; double-click the ring clears it.

### 5.4 Bypass and Solo
- Bypassed: power icon `stateOff` on `stateOffBg`; every other panel control fades to 38 % (✕ stays bright).
- Display: curve and Band handle stay visible but desaturated/faded; the Band is **excluded from the sum curve and
  the post Analyzer** (that data comes from the DSP).
- Solo: headphones next to Bypass, Band colour when on; one Band at a time. (Display treatment while soloed is up to
  the DSP author — the prototype only shows the state.)

### 5.5 Dynamics
- A Band becomes a Dynamic Band when its Dynamic Range ≠ 0 (drag the Gain ring or the ▲▼ grip on the display).
- Then three icons appear above Gain: ✕ Clear Dynamics, ⏻ Dynamics Bypass (neutral = active, red tint = bypassed),
  » open/close the dynamics section.
- Dynamics section (between Gain and Q): Threshold fader over a detector meter (top of travel = Auto, thumb shows
  "A"; drag only, double-click = Auto), Detection Source toggle (→|, lit = External/Sidechain), Detection Audition
  (🎧, Band colour when on), Detection Range toggle **Band ↔ Free**, Attack and Release knobs.
- **Free:** shows the Detection Range bar 30 px above the panel: neutral full-width line, band-colour segment between
  two pill handles (drag ends to set low/high, drag segment to move both, ←/→ nudge 1/6 oct), value labels above,
  soft band-colour column rising from it.

### 5.6 Header
- Loaded Preset name only (no folder), centred in the window, with the Modified dot when applicable; ‹ › step presets; click opens the preset browser (folders,
  search, favourites, preview curve, Save as / Copy / Paste).
- A/B Compare is one text button: active letter lit; click switches sides. "Copy" copies active → other (tooltip says which).

### 5.7 Footer
- **Analyzer ▾** opens the analyzer popover (§3). Label shows sources and "· Frozen" while frozen.
- **Global Bypass** (power icon, far left), then **Processing** (Processing Mode) and **Analyzer ▾**.
- **100 %** Gain Scale: vertical drag (Shift fine), ↑/↓ ±5, double-click 100 %.
- **0.0 dB** (Output Gain) opens the output popover: Output Gain knob (yellow arc, double-click 0 dB), Pan Mode chip
  (L/R ↔ M/S), centre-detented Output Pan slider with read-out ("Centre", "40 L", "20 S"…), toggles: Phase Invert Ø,
  Auto Gain A, meter. Hiding the meter widens the display.
- Popovers close on outside click.

### 5.8 Band colours
24 slots: hue order `[7,1,4,10,6,0,9,2,8,3,11,5] × 30° + 25°` → blue, orange, green, magenta, cyan, red, violet,
yellow, indigo, lime, pink, teal; `oklch(0.77 0.125 h)`. Slots 12–23 repeat lighter: `oklch(0.84 0.095 h+15°)`.
Pre-computed sRGB values are in `tokens.json` / `plugin/staple/Tokens.h`.

### 5.9 Auto-zoom
When any Band's Gain (× Gain Scale) exceeds the display range, step the range up to the smallest option that fits
(max ±30). Never zoom back in automatically.

### 5.10 Multi-selection
*In the prototype:* **Select All** (context menu, ⌘/Ctrl+A) and **Paste** create a multi-selection; every selected
Band handle shows the selected ring; right-click actions (Bypass, Invert Gain, Clear Dynamics, Shape, Slope,
Placement, Cut/Copy/Delete) apply to all of them; clicking a Band handle or empty space clears it. The Band panel shows
nothing while more than one band is selected.

*Proposed — not yet in the prototype, confirm before building:*
- Shift-click a Band handle toggles it in the selection; drag on empty space draws a marquee.
- Dragging any selected Band handle moves the group together: Frequency by the same ratio, Gain by the same offset
  (clamped per band).
- Arrow keys nudge every selected band; Delete removes them.

### 5.11 States not drawn in the prototype (build with these rules)
- **Spectrum Grab** (glossary feature, not designed yet): when built, hovering a peak in the Analyzer shows a ghost
  Band handle at that spot; dragging it creates a Band there (same visuals as the empty-state ghost). Confirm the
  interaction with the DSP author before building.
- **24-Band limit:** hide the empty-display ghost and ignore double-click once all 24 Band Slots are in use. Split is disabled
  with no free slot; Paste pastes only as many Bands as fit (its label shows the count) and is disabled at 24.
- **Save preset as…:** inline text field in the preset browser footer (same style as the knob type-in field),
  Enter saves into *User*, Esc cancels; name collisions append " 2".
- **Copy / Paste Band settings:** Copy is silent; Paste is disabled when the clipboard is empty; pasted Bands keep
  their frequencies and become the selection.
- **A/B Copy:** after copying, the button reads "Copied" for ~1 s.
- **Linear Phase / Natural Phase:** no extra UI in the design; if the DSP reports latency, a quiet `text3` "Latency 52 ms" may sit
  next to the Processing selector — only if the DSP author wants it.
- **No audio / Analyzer silent:** draw nothing (no flat line); peak hold decays as normal.

### 5.12 Global Bypass
Principle: **everything you hear and everything that moves stops, but the EQ stays visible and editable.**
- **Audio (DSP):** input passes straight through: no Bands, dynamics, Output Gain/Pan, Phase Invert or Auto Gain;
  click-free crossfade.
- **Curves:** every Band curve, fill and Dynamic Range wash fades to ~45 % and desaturates (the bypassed-Band
  treatment applied to all); the sum curve and its halo fade to 30 %. Shapes stay readable.
- **Band handles:** all drawn in the bypassed style; the selected one keeps its ring but loses its glow.
- **Dynamics:** all motion stops: the moving curve settles on Gain, the Dynamic Range ring's yellow Live Gain fill
  and the Threshold detector meters empty.
- **Analyzer:** keeps running (audio still flows); post = pre.
- **Output meter:** keeps metering (now the dry signal).
- **Footer:** power button `stateOff` on `stateOffBg`, a `stateOff` "Bypassed" label beside it, Gain Scale and Output
  Gain readouts in `text4` (not applied).
- **Band panel and all controls:** stay fully interactive at normal brightness; edits apply when un-bypassed.
- **Independence:** toggling Global Bypass never changes any Band's own Bypass or Dynamics Bypass.
- **Transition:** ~150 ms opacity fade on the display layers, matching the DSP crossfade.
- **Shortcut:** ⌘/Ctrl+B (only while the plugin window has keyboard focus).

## 6. Mock vs real

| Prototype element | Status | Real source in JUCE |
|---|---|---|
| Curves, sum, gain scale | **Real maths** (RBJ) | Same coefficients as DSP |
| Spectrum (pre/post), peak hold | Mock signal | FFT from audio thread via lock-free FIFO; post = after EQ |
| Sidechain spectrum | Mock (kick-like) | FFT of the sidechain bus (show when ≥ 1 band has sidechain on) |
| Gain-reduction envelope (ring yellow, live curve, detector meter) | Mock | Per-band detector/gain from DSP, atomics read at UI rate |
| Output meter, clip lights | Mock | Output peak/RMS from processBlock |
| Auto-gain −1.8 dB | Hard-coded | DSP loudness compensation value |
| Presets | Mock library | Real preset files / `juce::ValueTree` |

## 7. Accessibility
All controls have accessible names and values (see `aria-label`s in the prototype). Keyboard focus ring:
2 px `focus`, 2 px offset. Every drag control also works with arrow keys.

## 8. Assets
- Fonts: Manrope 400/500/600/700 (OFL) — embed via BinaryData.
- Logo: current PNG is low-res. Ask for SVG and render with `juce::Drawable`.
- Icons: 16 px grid, 1.5 px stroke, round caps/joins. Paths are in the prototype (`ICONS`, design-system board) —
  port them to `juce::Path` (SVG path strings parse with `Drawable::parseSVGPath`).

## 9. Scalable window (required)

The window **must** be resizable. Two mechanisms work together:

**A. UI scale (zoom)** — everything scales uniformly.
- A global scale factor `uiScale` (75 %, 100 %, 125 %, 150 %, 200 %, plus anything in between from the corner drag
  with Cmd/Ctrl held). Apply it once at the editor root (`setTransform(AffineTransform::scale(uiScale))` on a
  single content component, or design every metric as `token × uiScale`). Fonts, strokes, radii, icons and
  hit areas all scale; nothing is drawn as a bitmap that would blur.
- Persist `uiScale` per plugin instance (in the UI `ValueTree`) and as a global default for new instances.

**B. Free resize** — the display absorbs the space.
- Plain corner drag (`ResizableCornerComponent` + `ComponentBoundsConstrainer`) changes the window size at the current
  `uiScale`. Header (52 px) and footer (44 px) keep their height; the meter keeps its width; the **display grows/shrinks
  in both directions**. All display geometry is already relative to `W × H` (log-frequency x, dB y) — derive it from the
  display bounds, never from the 1134 × 612 design size.
- What stays fixed-size (× `uiScale`) inside a resized display: Band handles, the Band panel (stays centred at the
  bottom, 36 px up), hover card, tooltips, popovers, the range selector, the Detection Range bar (stays 30 px above the panel).
- Header: logo group left, preset selector **always centred in the window**, right group right — the two side groups
  share leftover width equally.
- **Minimum** content size 960 × 600 (at 100 %): below that the panel would collide with the frequency labels.
  **Maximum** = host/screen limits. Keep the aspect ratio free.
- Grid density adapts: hide minor grid lines and every other frequency label when the display is narrower than
  ~800 px; dB labels follow the display range options as now.
- Persist window width/height per instance.

**In the prototype:** drag the grip in the window's bottom-right corner to resize (double-click it for 1200 × 760),
and use the size icon at the far right of the footer for 75 / 100 / 125 / 150 %. Display size there is derived as
`W = window width − 66` (− 14 with the meter hidden) and `H = window height − 148`.

**Rendering:** use vector `Path` drawing throughout; if caching static layers (grid, knob faces), re-cache on resize and
on `uiScale` / display-scale change (`Component::getApproximateScaleFactorForComponent`) so HiDPI stays sharp.
Throttle expensive repaints during an active resize drag.

## 10. Acceptance checks
- [ ] A UI-control → parameter-ID mapping table exists; every mismatch with the DSP is listed for the DSP author, none invented.
- [ ] Every drag is wrapped in a change gesture (host automation writes one clean ramp per drag).
- [ ] Side-by-side with the prototype at 1×: Band panel, knobs, Band handles, header, footer match within 1–2 px.
- [ ] No hard-coded colours outside `plugin/staple/Tokens.h`.
- [ ] Panel width does not change when switching shapes/bands (only when the dynamics section opens).
- [ ] Bypassed band: visible but greyed, excluded from sum and post spectrum; panel fades to 38 %.
- [ ] Gain ring: red range, yellow live GR, flat lane; bypassing dynamics fades red to 30 % and hides yellow.
- [ ] Dragging a Band handle past the top zooms ±18 → ±30.
- [ ] Gain Scale 50 % halves every Band's Gain and Dynamic Range on screen (curve and Band handles follow the parameter).
- [ ] Hiding the meter widens the display; analyzer Freeze stops spectrum and peak hold only.
- [ ] Undo groups rapid drags into one step; 100 levels.
- [ ] Every control reachable and adjustable by keyboard.
- [ ] Global Bypass: curves and handles grey out, sum curve 30 %, dynamics stop, Analyzer post = pre, footer shows "Bypassed", Band panel still editable, Band Bypass states untouched.
- [ ] Window resizes freely from 960 × 600 upwards: display stretches, header/footer/panel keep their size, preset name stays centred.
- [ ] UI scale 75–200 % is crisp on 1× and 2× displays; size and scale restore when the session reopens.
