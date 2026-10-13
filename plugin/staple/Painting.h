#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <utility>
#include <vector>

namespace staple
{

// Drawings more than one control shares, each written once here.

// A shadow drawn without blur (HANDOFF.md §4): stacked translucent rounded rectangles, each a step
// wider, that add up to the shadow's colour inside the shape and fade out over its blur radius.
void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow);

// drawSoftShadow from images, for shadows drawn on every frame (the EQ display's handles): each shadow
// is drawn once per shape size, corner, shadow, scale and place within a device pixel, and drawn back
// from its image pixel for pixel. origin is where g's logical (0, 0) lies from a whole device pixel (the
// component's place in the window); where that isn't one at g's scale (startsOnWholeDevicePixel), the
// shadow is drawn directly.
class SoftShadowCache
{
public:
    void draw (juce::Graphics& g, juce::Point<float> origin, juce::Rectangle<float> shape, float cornerRadius, const juce::DropShadow& shadow);

private:
    struct Entry
    {
        juce::Rectangle<float> shape; // from the image's top left, in logical pixels
        float cornerRadius = 0.0f;
        juce::DropShadow shadow;
        float scale = 0.0f;
        juce::Image image;
    };
    std::vector<Entry> entries;
};

// A knob's face (the Knob and the look-and-feel's rotary slider): on its shadow, a gradient lit from
// 12 % below its top, ringed by a rim rimWidth wide. opacity dims the gradient only.
void drawKnobFace (juce::Graphics& g, juce::Rectangle<float> face, const juce::DropShadow& shadow, float rimWidth, juce::Colour rimColour,
                   float opacity = 1.0f);

// The meter's colours (the Output Meter and the Threshold fader), vertical: meter1 at bottomY, meter2
// and meter3 at stop2 and stop3 of the way up, meterClip from clipY up.
juce::ColourGradient meterGradient (float bottomY, float clipY, double stop2, double stop3);

// The readout card (the EQ display's handle readout and the knob tooltip): a title over a value on a
// menu-coloured card with its shadow and a hairline.
namespace readoutCard
{
    constexpr int paddingTop = 5, paddingSide = 10, paddingBottom = 6;
    constexpr int titleLine = 14, valueLine = 18;
    constexpr int height = paddingTop + titleLine + valueLine + paddingBottom;
} // namespace readoutCard
// The narrowest card that fits title and value.
float readoutCardWidth (const juce::String& title, const juce::String& value);
// The card in box; an empty value leaves its line blank (the knob tooltip while its value is typed).
void drawReadoutCard (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& title, const juce::String& value);

// A pill thumb (the Detection Range bar's handles and the Threshold fader's thumb): on its thumb
// shadow, a vertical gradient from top to bottom, with a 1 px highlight along its top inset by
// highlightInset at each end.
void drawPillThumb (juce::Graphics& g, juce::Rectangle<float> thumb, float cornerRadius, juce::Colour top, juce::Colour bottom,
                    float highlightInset);

// A radial highlight squashed vertically (the window's background and the Band panel's wash): colour
// at centre, fading to nothing radius across and verticalScale x radius down, through mid (a
// proportion of the radius and its colour) when given. Fills area, given unsquashed.
void fillSquashedRadial (juce::Graphics& g, juce::Rectangle<float> area, juce::Point<float> centre, float radius, float verticalScale,
                         juce::Colour colour, std::optional<std::pair<double, juce::Colour>> mid = std::nullopt);

} // namespace staple
