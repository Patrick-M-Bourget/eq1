#pragma once
// Staple EQ design tokens, generated from tokens.css / tokens.json. Regenerate rather than hand-edit.
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
    inline const juce::Colour text1 { 0xFFEEEBE5 };
    inline const juce::Colour text2 { 0xBDEEEBE5 };
    inline const juce::Colour text3 { 0x8AEEEBE5 };
    inline const juce::Colour text4 { 0x52EEEBE5 };
    inline const juce::Colour onLight { 0xFF121316 };
    inline const juce::Colour line1 { 0x0DFFFFFF };
    inline const juce::Colour line2 { 0x1AFFFFFF };
    inline const juce::Colour line3 { 0x2EFFFFFF };
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
    inline const juce::Colour edgeSelectorBase { 0xFF141519 };
    inline const juce::Colour knobFaceTop { 0xFF35373D }, knobFaceMid { 0xFF27282D }, knobFaceEdge { 0xFF1C1D21 };
    inline const juce::Colour dynRangeInner { 0xFF9C3344 }, dynLiveInner { 0xFFC49A34 };
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
    constexpr int windowWidth = 1200, windowHeight = 760;
} // namespace size

namespace motion
{
    constexpr int dur1Ms = 120, dur2Ms = 180, dur3Ms = 240; // hover/press, menus/cards, panels
} // namespace motion
} // namespace staple::tokens
