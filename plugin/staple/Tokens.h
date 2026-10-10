#pragma once
// Staple's design tokens: the values in docs/staple-handoff/tokens.json and HANDOFF.md's visual spec,
// kept by hand. The only place in plugin/ a colour is written as a number (scripts/check.sh docs).
#include <juce_graphics/juce_graphics.h>

namespace staple::tokens
{
namespace colour
{
    inline const juce::Colour bg0 { 0xFF0B0C0F };
    inline const juce::Colour surface1 { 0x09FFFFFF };
    inline const juce::Colour fill1 { 0x0FFFFFFF };
    inline const juce::Colour fill2 { 0x1AFFFFFF };
    inline const juce::Colour fill3 { 0x26FFFFFF };
    inline const juce::Colour raised { 0xD114161A };
    inline const juce::Colour menu { 0xFF16181C };
    // Contrast on bg0: 16.4, 9.1, 5.3 and 2.6 : 1. text4 is below readable contrast: disabled states
    // and decoration only, never text a user must read.
    inline const juce::Colour text1 { 0xFFEEEBE5 };
    inline const juce::Colour text2 { 0xBDEEEBE5 };
    inline const juce::Colour text3 { 0x8AEEEBE5 };
    inline const juce::Colour text4 { 0x52EEEBE5 };
    inline const juce::Colour onLight { 0xFF121316 };
    inline const juce::Colour line1 { 0x0DFFFFFF };
    inline const juce::Colour line2 { 0x1AFFFFFF };
    inline const juce::Colour line3 { 0x2EFFFFFF };
    inline const juce::Colour hoverCardLine { 0x17FFFFFF }; // white at 9 %
    inline const juce::Colour focus { 0xBFFFFFFF };
    inline const juce::Colour curveMain { 0xFFF5B930 };
    inline const juce::Colour curveMainHalo { 0x1AF5B930 };
    inline const juce::Colour anSc { 0xFF7FCFC4 };
    inline const juce::Colour dynRange { 0xFFD6455A };
    inline const juce::Colour dynLive { 0xFFF5C451 };
    inline const juce::Colour stateOff { 0xFFE5506A };
    inline const juce::Colour stateOffBg { 0x29E5506A };
    inline const juce::Colour placeLeft { 0xFFEEEBE5 };
    inline const juce::Colour placeRight { 0xFFE5604F };
    inline const juce::Colour placeStereo { 0xFFE9B44C };
    inline const juce::Colour placeMid { 0xFF5FCB76 };
    inline const juce::Colour placeSide { 0xFF4FA9E8 };
    inline const juce::Colour meter1 { 0xFF3FC79A };
    inline const juce::Colour meter2 { 0xFFA9D66A };
    inline const juce::Colour meter3 { 0xFFE9B44C };
    inline const juce::Colour meterClip { 0xFFE5604F };
    inline const juce::Colour meterClipOff { 0x14FFFFFF }; // white 8 %, an unlit Clip Light
    inline const juce::Colour meterTrack { 0x59000000 };   // black 35 %, under the Output Meter's bars
    inline const juce::Colour edgeSelectorBase { 0xFF141519 };
    inline const juce::Colour knobFaceTop { 0xFF35373D }, knobFaceMid { 0xFF27282D }, knobFaceEdge { 0xFF1C1D21 };
    inline const juce::Colour knobRim { 0x38FFFFFF };      // white 22 %
    inline const juce::Colour knobRimSmall { 0x33FFFFFF }; // white 20 %, on knobs of 30 px and below
    inline const juce::Colour knobRingLane { 0x0FFFFFFF }; // white 6 %
    inline const juce::Colour dynRangeInner { 0xFF9C3344 }, dynLiveInner { 0xFFC49A34 };
    inline const juce::Colour ringHint { 0xCCFFFFFF };     // the ring's ▲▼ grab hint, white 80 %
    // The Threshold fader: its track, its thumb (a grey gradient with a 1 px top highlight) and the
    // thumb's line, and the Detection Range bar's pill handles.
    inline const juce::Colour faderTrack { 0x59000000 };   // black 35 %
    inline const juce::Colour thumbTop { 0xFF4A4D55 }, thumbBottom { 0xFF34373E };
    inline const juce::Colour thumbHighlight { 0x2EFFFFFF }; // white 18 %
    inline const juce::Colour thumbLine { 0xD9F1EEE8 };      // rgba (241, 238, 232, 0.85)
    inline const juce::Colour pillTop { 0xFF5A5D64 }, pillBottom { 0xFF3A3D44 };
    inline const juce::Colour shadow { 0xFF000000 };
    // The Preset browser's modal: the scrim over the editor, and the dialog's fill and edge.
    inline const juce::Colour scrim { 0x59000000 };      // black 35 %
    inline const juce::Colour dialog { 0xF0131519 };     // rgba (19, 21, 25, 0.94)
    inline const juce::Colour dialogEdge { 0x14FFFFFF }; // white 8 %
    inline const juce::Colour none { 0x00000000 };       // nothing drawn: a widget's own fill or edge turned off
    // The window's three soft neutral highlights over bg0 (HANDOFF.md §4, "Window").
    inline const juce::Colour windowHighlight1 { 0x14E2E6EE }; // rgba (226, 230, 238, 0.08)
    inline const juce::Colour windowHighlight2 { 0x0DAAB2C0 }; // rgba (170, 178, 192, 0.05)
    inline const juce::Colour windowHighlight3 { 0x0F787E8A }; // rgba (120, 126, 138, 0.06)

    // The display: grid lines, and the Analyzer's spectra.
    inline const juce::Colour gridMinor { 0x06FFFFFF }; // white 2.5 %
    inline const juce::Colour gridMajor { 0x0FFFFFFF }; // white 6 %
    inline const juce::Colour gridZero { 0x38FFFFFF };  // white 22 %, the 0 dB line
    inline const juce::Colour anFillTop { 0x33C3CCE6 }, anFillMid { 0x12C3CCE6 }; // 20 % at the top, 7 % at 60 % down, then 0
    inline const juce::Colour anLine { 0x6BCBD3EA };    // the post spectrum, 42 %
    inline const juce::Colour anPeak { 0x29DDE3F5 };    // Peak Hold, 16 %
    inline const juce::Colour anPre { 0x33DDE3F5 };     // the pre spectrum beside the post one, 20 %
    inline const juce::Colour anScLine { 0x8C7FCFC4 };  // the Sidechain spectrum, anSc at 55 %

    // Band handles and the ghost Bell.
    inline const juce::Colour handleRing { 0x8C0A0B0E }; // rgba (10, 11, 14, 0.55), around an unselected handle
    inline const juce::Colour handleSelectedRing { 0xFFFFFFFF }; // and a selected one's, white
    inline const juce::Colour sheen { 0xFFFFFFFF };      // white light: a handle's sheen, the ghost Bell's line
    inline const juce::Colour ghostGlow { 0xFFFFC482 };  // rgb (255, 196, 130), at the ghost Bell's peak
} // namespace colour

// 24 band colours (slot 1-24) and their desaturated bypassed variants
inline const juce::Colour band[24] = {
    juce::Colour (0xFF58C0F8), juce::Colour (0xFFF19E63), juce::Colour (0xFF7FC982), juce::Colour (0xFFDC98E0), juce::Colour (0xFF28CBDA), juce::Colour (0xFFFA938C), juce::Colour (0xFFBAA4FB), juce::Colour (0xFFD9AD4C), juce::Colour (0xFF8DB2FF), juce::Colour (0xFFB2BE5A), juce::Colour (0xFFF291B8), juce::Colour (0xFF46CEB0),
    juce::Colour (0xFF9BD0FF), juce::Colour (0xFFF3C085), juce::Colour (0xFF92DEB5), juce::Colour (0xFFF5B3DE), juce::Colour (0xFF7FD9F5), juce::Colour (0xFFFFB69C), juce::Colour (0xFFDEBAF9), juce::Colour (0xFFD9CC83), juce::Colour (0xFFBEC4FF), juce::Colour (0xFFB6D795), juce::Colour (0xFFFFB1BC), juce::Colour (0xFF7ADFD8)
};
inline const juce::Colour bandBypassed[24] = {
    juce::Colour (0xFF8AAABD), juce::Colour (0xFFBC9E8A), juce::Colour (0xFF94AD94), juce::Colour (0xFFB49BB4), juce::Colour (0xFF84ADB2), juce::Colour (0xFFBF9A96), juce::Colour (0xFFA6A0BE), juce::Colour (0xFFB2A385), juce::Colour (0xFF97A5C2), juce::Colour (0xFFA3A888), juce::Colour (0xFFBD99A6), juce::Colour (0xFF88AEA3),
    juce::Colour (0xFF90A7C0), juce::Colour (0xFFB7A087), juce::Colour (0xFF8DAE9B), juce::Colour (0xFFB99AAE), juce::Colour (0xFF86ACB8), juce::Colour (0xFFBE9C90), juce::Colour (0xFFAD9DBA), juce::Colour (0xFFABA686), juce::Colour (0xFF9EA2C1), juce::Colour (0xFF9BAB8D), juce::Colour (0xFFBF999E), juce::Colour (0xFF85AEAB)
};

namespace size
{
    constexpr float fs1 = 10.0f;
    constexpr float fs2 = 11.0f;
    constexpr float fs3 = 12.0f;
    constexpr float fs4 = 13.0f;
    constexpr float fs5 = 16.0f;
    constexpr float r1 = 4.0f;
    constexpr float r2 = 6.0f;
    constexpr float r3 = 10.0f;
    constexpr float iconStroke = 1.5f;
    constexpr float iconGrid = 16.0f;
    constexpr float focusWidth = 2.0f, focusOffset = 2.0f;
} // namespace size

// The window and its areas at 100 % UI scale (HANDOFF.md §2, §4, §9).
namespace layout
{
    constexpr int outerPadding = 14, gap = 12;
    constexpr int headerHeight = 52, footerHeight = 44, meterWidth = 40;
    constexpr int displayWidth = 1134, displayWidthWithoutMeter = 1186, displayHeight = 612;
    // A new editor's window; it resizes between the minimum, below which the Band panel would collide
    // with the Frequency labels, and the maximum.
    constexpr int windowWidth = 1200, windowHeight = 760;
    constexpr int minimumWidth = 960, minimumHeight = 600, maximumWidth = 2560, maximumHeight = 1600;
    // The display's edges dissolve over these distances (grid, Analyzer and curve fills only).
    constexpr float fadeTop = 18.0f, fadeBottom = 84.0f, fadeLeft = 36.0f, fadeRight = 56.0f;
    // Below this display width the grid drops its minor lines and every other Frequency label.
    constexpr int narrowDisplayWidth = 800;
    constexpr int bandPanelAboveBottom = 36, bandPanelBell = 22;
    constexpr int bandPanelPaddingTop = 22, bandPanelPaddingSide = 26, bandPanelPaddingBottom = 10;
    constexpr int detectionRangeBarAbovePanel = 30;
    constexpr int edgeSelectorWidth = 104, edgeSelectorHeight = 34, slopeButtonWidth = 90;
    constexpr int iconButton = 24;
    // The dynamics: the icon row above Gain, the section between Gain and Q, and its Threshold fader.
    constexpr int dynamicsIcon = 22, dynamicsIconsAbove = 42, dynamicsIconGap = 6;
    constexpr int dynamicsSectionWidth = 134, dynamicsSectionHeight = 108, dynamicsSectionLift = 14;
    constexpr int dynamicsSectionPaddingY = 6, dynamicsSectionPaddingX = 10, dynamicsSectionGap = 10;
    constexpr int faderWidth = 26, faderTrack = 80, faderTrackWidth = 7, faderThumbHeight = 16;
    constexpr int dynamicsColumnWidth = 78, detectionRangeButtonHeight = 24;
    // The Detection Range bar: its pill handles, the segment's hit height, the labels above and the column.
    constexpr int pillWidth = 12, pillHeight = 20, segmentHitHeight = 12, segmentHeight = 3;
    constexpr int detectionLabelAbove = 26, detectionColumnHeight = 190;
    constexpr int menuItemHeight = 30, menuPadding = 6, menuSeparatorMargin = 5;
} // namespace layout

// Knobs and the Dynamic Range ring around the Gain knob.
namespace knob
{
    constexpr float frequency = 50.0f, gain = 66.0f, q = 50.0f, small = 30.0f;
    constexpr float rim = 2.0f, rimSmall = 1.5f;           // rimSmall at 30 px and below
    constexpr float arc = 3.0f, arcSmall = 2.0f;           // arcSmall below 40 px
    constexpr float arcInset = 4.5f, arcInsetProportion = 0.1f; // the arc's radius is r - max (arcInset, d x arcInsetProportion)
    constexpr float sweepDegrees = 270.0f;
    constexpr float arcGlow = 1.5f, arcGlowAlpha = 0.3f;   // a wider translucent stroke under the arc
    constexpr float originStubDegrees = 0.5f;              // what the arc shows at its origin
    constexpr float dragPixels = 200.0f, fineDragPixels = 800.0f; // the full range, and with Shift
    constexpr float ringLane = 12.0f, ringOffset = 10.0f;
    constexpr float ringRangeAlpha = 0.85f, ringRangeBypassedAlpha = 0.3f; // the range arc, and under Dynamics Bypass
    constexpr float liveGainMinimum = 0.05f; // dB: the Live Gain arc hides below this movement
    constexpr float ringDbPerDrag = 60.0f;   // dB per dragPixels (fineDragPixels with Shift)
} // namespace knob

// The dynamics section and the Detection Range bar: the meter gradient behind the Threshold fader's
// track (its stops at 55 % and 78 % up), and the column of the Band's colour over the bar.
namespace dynamics
{
    constexpr float meterBehindAlpha = 0.16f, meterStop2 = 0.55f, meterStop3 = 0.78f;
    constexpr float columnAlpha = 0.16f;
} // namespace dynamics

// Band handles on the display (HANDOFF.md §4 "Band handles"): sizes in px, alphas 0 to 1.
namespace handle
{
    constexpr float diameter = 16.0f, hoverScale = 1.15f, selectedDiameter = 22.0f;
    constexpr float selectedRing = 2.0f, glow = 10.0f, glowAlpha = 0.4f, sheenAlpha = 0.16f;
    constexpr float sheenReach = 0.7f;                            // of the radius, where the sheen reaches 0
    constexpr float ring = 1.0f;                                  // handleRing, around an unselected handle
    constexpr float bypassedAlpha = 0.85f, bypassedRingAlpha = 0.6f; // a Bypassed handle, and its selected ring
    constexpr float hitRadius = 9.0f;                             // at least, from the centre
    constexpr float soloRing = 2.0f, soloRingOffset = 5.0f;       // the Solo cue's ring, outside the handle
} // namespace handle

// The Dynamic Range Handle on the display: two triangles in the Band's colour, in a hit area.
namespace dynamicRangeHandle
{
    constexpr float triangleWidth = 10.0f, triangleHeight = 7.0f, gap = 3.0f;
    constexpr float width = 18.0f, height = 26.0f;
    constexpr float belowHandle = 26.0f, edgeInset = 14.0f; // with no Dynamic Range; kept inside the top and bottom
    constexpr float restingAlpha = 0.55f;                   // other Dynamic Bands' handles, until hovered
} // namespace dynamicRangeHandle

// The ghost Bell over empty space (HANDOFF.md §5.1).
namespace ghost
{
    constexpr double q = 1.0, minimumGainProportion = 0.12; // of the Display Range, away from 0 dB
    constexpr float peakClearance = 60.0f, span = 190.0f;   // the peak from the top and bottom; the Bell either side
    constexpr float stroke = 1.75f, strokeAlpha = 0.38f;
    constexpr float lineTopAlpha = 0.03f, lineMiddleAlpha = 0.16f, lineBottomAlpha = 0.1f;
    constexpr float glowDiameter = 90.0f, glowAlpha = 0.16f, glowMiddleAlpha = 0.05f;
    constexpr float readoutAboveBottom = 10.0f, labelClearance = 46.0f, fadedLabelAlpha = 0.12f;
    constexpr int fadeInMs = 600;
} // namespace ghost

// The Hover Card over a Band's handle (HANDOFF.md §2 "HoverCard", §5.2), in px.
namespace hoverCard
{
    constexpr int width = 172, height = 70, padding = 8;
    constexpr int gap = 18;       // between the card and its handle's centre
    constexpr int inset = 6;      // from the display's left and right
    constexpr int roomAbove = 100; // above its handle when the handle is further than this from the display's top
    constexpr float tip = 9.0f;   // the arrow tip's square, turned 45° and half showing
    constexpr float bypassedAlpha = 0.38f;
    constexpr int restMs = 300;   // the pointer's rest on a handle before the card shows
} // namespace hoverCard

// Curves on the display (HANDOFF.md §4 "Display", §5.12): widths in px, alphas 0 to 1.
namespace curve
{
    constexpr float line = 1.0f, lineAlpha = 0.5f, fillAlpha = 0.1f;                  // other Bands
    constexpr float hoverLine = 1.5f, hoverLineAlpha = 0.95f, hoverFillAlpha = 0.26f; // other Bands on hover
    constexpr float selectedLine = 1.5f, selectedFillAlpha = 0.3f;
    constexpr float glow = 2.0f, glowAlpha = 0.3f; // around the selected Band's line, as stacked strokes
    constexpr float bypassedSelectedLineAlpha = 0.5f, bypassedSelectedFillAlpha = 0.14f;
    constexpr float bypassedScale = 0.6f; // other Bypassed Bands, of their usual alphas
    constexpr float washAlpha = 0.2f, bypassedWashAlpha = 0.1f; // the Dynamic Range wash, in dynRange
    constexpr float globalBypassScale = 0.45f, globalBypassSumAlpha = 0.3f;
    // The sum: a line over its halo, and a wider, fainter halo in place of a blur.
    constexpr float sum = 2.0f, sumHalo = 4.0f, sumOuterHalo = 8.0f, sumOuterHaloAlpha = 0.5f;
} // namespace curve

// Shadows: CSS blur radius and offset, as juce::DropShadow takes them.
namespace shadow
{
    inline const juce::DropShadow shadow1 { colour::shadow.withAlpha (0.45f), 24, { 0, 8 } };  // menus, popovers, tooltips
    inline const juce::DropShadow shadow2 { colour::shadow.withAlpha (0.55f), 48, { 0, 18 } }; // the context menu, dialogs
    inline const juce::DropShadow knob { colour::shadow.withAlpha (0.4f), 14, { 0, 6 } };
    inline const juce::DropShadow knobSmall { colour::shadow.withAlpha (0.35f), 8, { 0, 3 } }; // 30 px and below
    inline const juce::DropShadow handle { colour::shadow.withAlpha (0.45f), 5, { 0, 0 } };
    inline const juce::DropShadow thumb { colour::shadow.withAlpha (0.5f), 5, { 0, 2 } }; // the fader's thumb, the bar's pills
    inline const juce::DropShadow panThumb { colour::shadow.withAlpha (0.5f), 3, { 0, 1 } }; // Output Pan's small thumb
    inline const juce::DropShadow selectedHandle { colour::shadow.withAlpha (0.45f), 8, { 0, 0 } };
} // namespace shadow

namespace motion
{
    constexpr int dur1Ms = 120, dur2Ms = 180, dur3Ms = 240; // hover/press, menus/cards, panels
    constexpr int hoverFadeMs = 220;                        // a Band curve's hover fade, and the hover card's hide delay
    constexpr int globalBypassFadeMs = 150;                 // the display's curves fading into and out of Global Bypass
    // The one easing curve: cubic-bezier (0.2, 0.7, 0.2, 1).
    constexpr float easeX1 = 0.2f, easeY1 = 0.7f, easeX2 = 0.2f, easeY2 = 1.0f;
    // Menus, popovers and cards pop in from 3 px lower at 98.5 % scale.
    constexpr float popInOffset = 3.0f, popInScale = 0.985f;
    constexpr float hoverBrightness = 1.18f, pressedBrightness = 1.3f, disabledAlpha = 0.35f;
} // namespace motion
} // namespace staple::tokens
