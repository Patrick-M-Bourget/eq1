#pragma once

#include "Fonts.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace staple
{

// Staple's look for every stock JUCE widget (HANDOFF.md §1, §4, §7): token colours for every stock
// ColourId, Manrope throughout, and Staple's menus, buttons, toggles, combo boxes, knobs, sliders,
// labels, text fields, scrollbars, tooltips and focus ring. Set once on the editor; PopupMenus, which
// are windows of their own, take it from the component that shows them.
//
// Hover lights a control up (brightness x 1.18, pressed x 1.3) and never adds an outline; disabled
// controls dim to 35 %.
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont (juce::Slider& slider) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool down, int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    int getPopupMenuBorderSize() override;
    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth,
                                    int& idealHeight) override;
    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) override;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle,
                           float endAngle, juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position, float minPosition,
                           float maxPosition, juce::Slider::SliderStyle style, juce::Slider& slider) override;
    int getSliderThumbRadius (juce::Slider& slider) override;

    void drawLabel (juce::Graphics& g, juce::Label& label) override;
    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;
    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;

    int getDefaultScrollbarWidth() override;
    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPosition,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;

    // A 2 px focus ring, 2 px outside the component.
    std::unique_ptr<juce::FocusOutline> createFocusOutlineForComponent (juce::Component& component) override;

private:
    // Keeps the typefaces loaded while the editor is open, so staple::font doesn't load them again.
    juce::SharedResourcePointer<Typefaces> typefaces;
};

} // namespace staple
