#include "Icons.h"

#include "Tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace staple
{

namespace
{
struct Definition
{
    const char* name;
    float width, height; // the SVG viewBox
    const char* path;
};

// In the order of Icon, with the prototype's viewBox and path (Main.dc.html and the design system board).
const Definition definitions[numIcons] = {
    { "power", 14, 14, "M7 1.5 V6.5 M3.8 3.6 a4.6 4.6 0 1 0 6.4 0" },
    { "close", 12, 12, "M2 2 L10 10 M10 2 L2 10" },
    { "more", 10, 10, "M2 3.5 L5 6.5 L8 3.5" },
    { "dropdown", 10, 10, "M2.5 4 L5 6.5 L7.5 4" },
    { "chevronUp", 10, 10, "M2.5 6.5 L5 4 L7.5 6.5" },
    { "submenu", 8, 10, "M2.5 2 L5.5 5 L2.5 8" },
    { "check", 10, 10, "M1.5 5.2 L4 7.5 L8.5 2.5" },
    { "previous", 16, 16, "M10 3 L5 8 L10 13" },
    { "next", 16, 16, "M6 3 L11 8 L6 13" },
    { "undo", 18, 18, "M6 4 L3 7 L6 10 M3 7 H11 a4 4 0 0 1 0 8 H8" },
    { "redo", 18, 18, "M12 4 L15 7 L12 10 M15 7 H7 a4 4 0 0 0 0 8 H10" },
    { "headphones", 16, 16, "M2.5 10 V8.5 a5.5 5.5 0 0 1 11 0 V10 M2 10 H5 V14 H2 Z M11 10 H14 V14 H11 Z" },
    { "detectionSource", 16, 12, "M1 6 H9 M6 3 L9 6 L6 9 M12.5 1.5 V10.5" },
    { "track", 16, 12, "M1 6 H3 L4.5 2.5 L6.5 9.5 L8.5 3.5 L10 8 L11.5 6 H15" },
    { "freeze", 14, 14, "M7 1 V13 M1.8 4 L12.2 10 M1.8 10 L12.2 4 M5.5 2 L7 3.3 L8.5 2 M5.5 12 L7 10.7 L8.5 12" },
    { "dynamicsOpen", 16, 16, "M4 3 L9 8 L4 13 M8 3 L13 8 L8 13" },
    { "phaseInvert", 16, 16, "M3 8 a5 5 0 1 0 10 0 a5 5 0 1 0 -10 0 M3 13 L13 3" },
    { "meter", 14, 14, "M4 2 V12 M7 5 V12 M10 3 V12" },
    { "uiScale", 14, 14, "M8.5 1.5 H12.5 V5.5 M12.5 1.5 L8 6 M5.5 12.5 H1.5 V8.5 M1.5 12.5 L6 8" },
    { "search", 16, 16, "M2.2 7 a4.8 4.8 0 1 0 9.6 0 a4.8 4.8 0 1 0 -9.6 0 M10.6 10.6 L14 14" },
    { "star", 16, 16, "M8 1.8 L9.9 5.7 L14.1 6.3 L11 9.2 L11.8 13.4 L8 11.4 L4.2 13.4 L5 9.2 L1.9 6.3 L6.1 5.7 Z" },
    { "peakHold", 16, 12, "M1 11 L4 5 L6 7 L9 2 L12 6 L15 4 M1 11 L4 8 L6 9.5 L9 6 L12 9 L15 8" },
    { "resizeGrip", 16, 16, "M14 6 L6 14 M14 10 L10 14" },
    { "dynamicRangeHandleUp", 10, 7, "M5 0.5 L9.5 6.5 H0.5 Z" },
    { "dynamicRangeHandleDown", 10, 7, "M5 6.5 L9.5 0.5 H0.5 Z" },
    { "bell", 20, 12, "M1 10 C6 10 7 2 10 2 S14 10 19 10" },
    { "lowShelf", 20, 12, "M1 3 H6 C9 3 11 9 14 9 H19" },
    { "highShelf", 20, 12, "M1 9 H6 C9 9 11 3 14 3 H19" },
    { "lowCut", 20, 12, "M2 11 C3.5 5 5.5 3 9 3 H19" },
    { "highCut", 20, 12, "M1 3 H11 C14.5 3 16.5 5 18 11" },
    { "notch", 20, 12, "M1 3 H7.5 C9 3 9.2 11 10 11 C10.8 11 11 3 12.5 3 H19" },
    { "bandPass", 20, 12, "M1 11 C7 11 8 2 10 2 S13 11 19 11" },
    { "tiltShelf", 20, 12, "M1 8.5 H5 C8.5 8.5 11.5 3.5 15 3.5 H19 M8 6 H12" },
    { "flatTilt", 20, 12, "M1 10 L19 2" },
    { "allPass", 20, 12, "M1 6 H19 M10 3 V9" },
    { "placementStereo", 20, 12, "M2.5 6 a4.5 4.5 0 1 0 9 0 a4.5 4.5 0 1 0 -9 0 M8.5 6 a4.5 4.5 0 1 0 9 0 a4.5 4.5 0 1 0 -9 0" },
    { "placementLeft", 20, 12, "M2.5 6 a4.5 4.5 0 1 0 9 0 a4.5 4.5 0 1 0 -9 0" },
    { "placementRight", 20, 12, "M8.5 6 a4.5 4.5 0 1 0 9 0 a4.5 4.5 0 1 0 -9 0" },
    { "placementMid", 20, 12, "M10 2.65 A4.5 4.5 0 0 1 10 9.35 A4.5 4.5 0 0 1 10 2.65" },
    { "placementSide", 20, 12, "M10 2.65 A4.5 4.5 0 1 0 10 9.35 A4.5 4.5 0 0 1 10 2.65 M10 2.65 A4.5 4.5 0 1 1 10 9.35 A4.5 4.5 0 0 0 10 2.65" },
    { "detectionRangeBand", 16, 12, "M1 10 C5 10 6 3 8 3 S11 10 15 10" },
    { "detectionRangeFree", 16, 12, "M2 6 H14 M4.5 3.5 L2 6 L4.5 8.5 M11.5 3.5 L14 6 L11.5 8.5" }
};

const Definition& definitionOf (Icon icon)
{
    return definitions[static_cast<size_t> (icon)];
}

juce::AffineTransform fitting (Icon icon, juce::Rectangle<float> area)
{
    return juce::RectanglePlacement (juce::RectanglePlacement::centred).getTransformToFit (gridOf (icon), area);
}
} // namespace

const char* nameOf (Icon icon)
{
    return definitionOf (icon).name;
}

const juce::Path& pathOf (Icon icon)
{
    static const auto paths = [] {
        std::array<juce::Path, numIcons> parsed;
        for (size_t i = 0; i < parsed.size(); ++i)
            parsed[i] = juce::Drawable::parseSVGPath (definitions[i].path);
        return parsed;
    }();
    return paths[static_cast<size_t> (icon)];
}

juce::Rectangle<float> gridOf (Icon icon)
{
    const auto& definition = definitionOf (icon);
    return { definition.width, definition.height };
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour (colour);
    g.strokePath (pathOf (icon),
                  juce::PathStrokeType (tokens::size::iconStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  fitting (icon, area));
}

void fillIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour (colour);
    g.fillPath (pathOf (icon), fitting (icon, area));
}

} // namespace staple
