#include "LookAndFeel.h"

#include "Icons.h"
#include "Tokens.h"

#include <cmath>

namespace staple
{

namespace colour = tokens::colour;
namespace size = tokens::size;
namespace motion = tokens::motion;

namespace
{
// A Label's default height, 15, is drawn at fs3; other heights in proportion.
constexpr float defaultLabelHeight = 15.0f;

// The column at the left of a menu item for its tick or icon, and the padding either side.
constexpr float menuTickColumn = 16.0f, menuItemPadding = 10.0f;

// Hover and press light a fill up: an opaque colour brightens, a translucent one (fill1, fill2) grows
// more opaque, which is what brightening it over the dark background looks like.
juce::Colour lit (juce::Colour colour, bool highlighted, bool down)
{
    const float factor = down ? motion::pressedBrightness : highlighted ? motion::hoverBrightness : 1.0f;
    return colour.isOpaque() ? colour.withMultipliedBrightness (factor) : colour.withMultipliedAlpha (factor);
}

float enabledAlpha (const juce::Component& component)
{
    return component.isEnabled() ? 1.0f : motion::disabledAlpha;
}

Weight weightOf (const juce::Font& font)
{
    const auto style = font.getTypefaceStyle();
    if (style.containsIgnoreCase ("SemiBold"))
        return Weight::semiBold;
    if (font.isBold() || style.containsIgnoreCase ("Bold"))
        return Weight::bold;
    if (style.containsIgnoreCase ("Medium"))
        return Weight::medium;
    return Weight::regular;
}
} // namespace

LookAndFeel::LookAndFeel()
    : juce::LookAndFeel_V4 ({ colour::bg0, colour::fill1, colour::menu, colour::line2, colour::text1, colour::fill3,
                              colour::text1, colour::fill2, colour::text1 })
{
    setDefaultSansSerifTypeface (typefaces->weights[0]);

    // The stock ColourIds the scheme above doesn't set as Staple wants them.
    setColour (juce::ResizableWindow::backgroundColourId, colour::bg0);
    setColour (juce::TextButton::buttonColourId, colour::fill1);
    setColour (juce::TextButton::buttonOnColourId, colour::fill3);
    setColour (juce::TextButton::textColourOffId, colour::text2);
    setColour (juce::TextButton::textColourOnId, colour::text1);
    setColour (juce::ToggleButton::textColourId, colour::text2);
    setColour (juce::ToggleButton::tickColourId, colour::onLight);
    setColour (juce::ToggleButton::tickDisabledColourId, colour::line3);
    setColour (juce::ComboBox::backgroundColourId, colour::fill1);
    setColour (juce::ComboBox::textColourId, colour::text1);
    setColour (juce::ComboBox::outlineColourId, colour::line2);
    setColour (juce::ComboBox::buttonColourId, colour::fill1);
    setColour (juce::ComboBox::arrowColourId, colour::text3);
    setColour (juce::ComboBox::focusedOutlineColourId, colour::focus);
    // A hair short of opaque, so a menu's window is transparent and its corners can be round.
    setColour (juce::PopupMenu::backgroundColourId, colour::menu.withAlpha (0.99f));
    setColour (juce::PopupMenu::textColourId, colour::text1);
    setColour (juce::PopupMenu::headerTextColourId, colour::text3);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::fill1);
    setColour (juce::PopupMenu::highlightedTextColourId, colour::text1);
    setColour (juce::Label::textColourId, colour::text2);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundWhenEditingColourId, colour::menu);
    setColour (juce::Label::textWhenEditingColourId, colour::text1);
    setColour (juce::Label::outlineWhenEditingColourId, colour::line3);
    setColour (juce::Slider::backgroundColourId, colour::line3);
    setColour (juce::Slider::trackColourId, colour::text2);
    setColour (juce::Slider::thumbColourId, colour::text1);
    setColour (juce::Slider::rotarySliderFillColourId, colour::text1);
    setColour (juce::Slider::rotarySliderOutlineColourId, colour::knobRim);
    setColour (juce::Slider::textBoxTextColourId, colour::text2);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colour::fill3);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, colour::fill1);
    setColour (juce::TextEditor::textColourId, colour::text1);
    setColour (juce::TextEditor::highlightColourId, colour::fill3);
    setColour (juce::TextEditor::highlightedTextColourId, colour::text1);
    setColour (juce::TextEditor::outlineColourId, colour::line2);
    setColour (juce::TextEditor::focusedOutlineColourId, colour::line3);
    setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, colour::text1);
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, colour::fill3);
    setColour (juce::TooltipWindow::backgroundColourId, colour::menu);
    setColour (juce::TooltipWindow::textColourId, colour::text1);
    setColour (juce::TooltipWindow::outlineColourId, colour::line2);
    setColour (juce::AlertWindow::backgroundColourId, colour::menu);
    setColour (juce::AlertWindow::textColourId, colour::text1);
    setColour (juce::AlertWindow::outlineColourId, colour::line2);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, colour::line2);
    setColour (juce::ListBox::textColourId, colour::text1);
    setColour (juce::HyperlinkButton::textColourId, colour::text1);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typefaces->weights[static_cast<size_t> (weightOf (font))];
    return juce::LookAndFeel_V4::getTypefaceForFont (font);
}

juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    const auto own = label.getFont();
    return font (size::fs3 * own.getHeight() / defaultLabelHeight, weightOf (own));
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font (size::fs3, Weight::medium);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (size::fs3);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font (size::fs4);
}

juce::Font LookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return font (size::fs4);
}

juce::Font LookAndFeel::getAlertWindowTitleFont()
{
    return font (size::fs5, Weight::semiBold);
}

juce::Font LookAndFeel::getAlertWindowMessageFont()
{
    return font (size::fs4);
}

juce::Font LookAndFeel::getAlertWindowFont()
{
    return font (size::fs3);
}

//==============================================================================
void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                        bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat();
    // A transparent button shows a fill2 box only while hovered (A/B Compare, Copy).
    auto fill = backgroundColour.isTransparent() ? (highlighted || down ? colour::fill2 : backgroundColour)
                                                 : lit (backgroundColour, highlighted, down);
    g.setColour (fill.withMultipliedAlpha (enabledAlpha (button)));
    juce::Path shape;
    shape.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), size::r2, size::r2,
                               ! (button.isConnectedOnLeft() || button.isConnectedOnTop()),
                               ! (button.isConnectedOnRight() || button.isConnectedOnTop()),
                               ! (button.isConnectedOnLeft() || button.isConnectedOnBottom()),
                               ! (button.isConnectedOnRight() || button.isConnectedOnBottom()));
    g.fillPath (shape);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    const auto text = button.getToggleState() || highlighted ? button.findColour (juce::TextButton::textColourOnId)
                                                             : button.findColour (juce::TextButton::textColourOffId);
    g.setColour (text.withMultipliedAlpha (enabledAlpha (button)));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (6, 2), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    const float alpha = enabledAlpha (button);
    const bool on = button.getToggleState();
    constexpr float boxSize = 14.0f;
    const auto box = juce::Rectangle<float> (boxSize, boxSize).withCentre ({ 2.0f + boxSize / 2.0f, button.getHeight() / 2.0f });
    if (on)
    {
        g.setColour (lit (colour::text1, highlighted, down).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (box, size::r1);
        drawIcon (g, Icon::check, box.reduced (3.0f), button.findColour (juce::ToggleButton::tickColourId).withMultipliedAlpha (alpha));
    }
    else
    {
        g.setColour ((highlighted ? colour::text3 : colour::line3).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (box.reduced (0.5f), size::r1, 1.0f);
    }

    // The label lights up to text1 on hover, and stays lit while on.
    g.setColour ((on || highlighted ? colour::text1 : button.findColour (juce::ToggleButton::textColourId)).withMultipliedAlpha (alpha));
    g.setFont (font (size::fs3));
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().withTrimmedLeft (juce::roundToInt (box.getRight()) + 6),
                      juce::Justification::centredLeft, 1);
}

//==============================================================================
void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int buttonX, int buttonY, int buttonW,
                                int buttonH, juce::ComboBox& box)
{
    const float alpha = enabledAlpha (box);
    const auto bounds = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (lit (box.findColour (juce::ComboBox::backgroundColourId), box.isMouseOver (true), down).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (bounds, size::r2);
    const auto arrow = juce::Rectangle<int> (buttonX, buttonY, buttonW, buttonH).toFloat().withSizeKeepingCentre (10.0f, 10.0f);
    drawIcon (g, Icon::dropdown, arrow, box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (alpha));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
int LookAndFeel::getPopupMenuBorderSize()
{
    return tokens::layout::menuPadding;
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle (bounds, size::r3);
    g.setColour (colour::line2);
    g.drawRoundedRectangle (bounds.reduced (0.5f), size::r3, 1.0f);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                     bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                     const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colour::fill2);
        const auto line = area.toFloat().reduced (8.0f, 0.0f);
        g.fillRect (line.withSizeKeepingCentre (line.getWidth(), 1.0f));
        return;
    }

    auto row = area.toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle (row, size::r2);
    }
    // Unavailable items dim like any disabled control.
    const auto base = textColour != nullptr ? *textColour : findColour (juce::PopupMenu::textColourId);
    const auto ink = isActive ? base : base.withMultipliedAlpha (motion::disabledAlpha);

    row.removeFromLeft (menuItemPadding);
    row.removeFromRight (menuItemPadding);
    const auto tick = row.removeFromLeft (menuTickColumn).withSizeKeepingCentre (10.0f, 10.0f);
    if (icon != nullptr)
        icon->drawWithin (g, tick, juce::RectanglePlacement::centred, ink.getFloatAlpha());
    else if (isTicked)
        drawIcon (g, Icon::check, tick, ink);

    if (hasSubMenu)
        drawIcon (g, Icon::submenu, row.removeFromRight (8.0f).withSizeKeepingCentre (8.0f, 10.0f), ink.withMultipliedAlpha (0.55f));
    else if (shortcutKeyText.isNotEmpty())
    {
        g.setFont (font (size::fs2));
        g.setColour (colour::text3.withMultipliedAlpha (isActive ? 1.0f : motion::disabledAlpha));
        g.drawText (shortcutKeyText, row, juce::Justification::centredRight);
    }

    g.setFont (getPopupMenuFont());
    g.setColour (ink);
    g.drawFittedText (text, row.toNearestInt(), juce::Justification::centredLeft, 1);
}

void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = 1 + 2 * tokens::layout::menuSeparatorMargin;
        return;
    }
    idealHeight = tokens::layout::menuItemHeight;
    // Room for the tick column, padding either side, and a submenu chevron or shortcut.
    idealWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), text) + menuTickColumn
                                   + 2.0f * menuItemPadding + 24.0f);
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName)
{
    g.setFont (font (size::fs2));
    g.setColour (findColour (juce::PopupMenu::headerTextColourId));
    g.drawFittedText (sectionName, area.withTrimmedLeft (static_cast<int> (menuItemPadding)).withTrimmedRight (static_cast<int> (menuItemPadding)),
                      juce::Justification::bottomLeft, 1);
}

//==============================================================================
void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle,
                                    float endAngle, juce::Slider& slider)
{
    const float alpha = enabledAlpha (slider);
    // The knob's circle sits where LookAndFeel_V4 draws it, so overlays placed around it stay put.
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10.0f);
    const float diameter = std::min (area.getWidth(), area.getHeight());
    if (diameter <= 0.0f)
        return;
    const auto face = area.withSizeKeepingCentre (diameter, diameter);

    juce::Path circle;
    circle.addEllipse (face);
    tokens::shadow::knob.drawForPath (g, circle);

    // Lit from the top: the face's gradient is centred 12 % down from its top edge.
    juce::ColourGradient gradient (colour::knobFaceTop, face.getCentreX(), face.getY() + 0.12f * diameter,
                                   colour::knobFaceEdge, face.getCentreX(), face.getY() + 1.32f * diameter, true);
    gradient.addColour (0.45, colour::knobFaceMid);
    g.setGradientFill (gradient);
    g.setOpacity (alpha);
    g.fillEllipse (face);

    const float rim = diameter <= tokens::knob::small ? tokens::knob::rimSmall : tokens::knob::rim;
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId).withMultipliedAlpha (alpha));
    g.drawEllipse (face.reduced (rim / 2.0f), rim);

    // Only the active arc, no track; a knob whose range is centred on 0 starts from 12 o'clock.
    const float radius = diameter / 2.0f - std::max (tokens::knob::arcInset, std::round (diameter * tokens::knob::arcInsetProportion));
    const bool bipolar = slider.getMinimum() < 0.0 && juce::approximatelyEqual (slider.getMinimum(), -slider.getMaximum());
    const float from = bipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const float to = startAngle + position * (endAngle - startAngle);
    if (std::abs (to - from) < 1.0e-3f)
        return;
    juce::Path arc;
    arc.addCentredArc (face.getCentreX(), face.getCentreY(), radius, radius, 0.0f, from, to, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withMultipliedAlpha (alpha));
    g.strokePath (arc, juce::PathStrokeType (diameter < 40.0f ? tokens::knob::arcSmall : tokens::knob::arc,
                                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position, float minPosition,
                                    float maxPosition, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.isBar() || slider.isTwoValue() || slider.isThreeValue())
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, position, minPosition, maxPosition, style, slider);
        return;
    }
    const float alpha = enabledAlpha (slider);
    const bool horizontal = slider.isHorizontal();
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto along = [&] (float at) {
        return horizontal ? juce::Point<float> (at, area.getCentreY()) : juce::Point<float> (area.getCentreX(), at);
    };
    const juce::Point<float> start = horizontal ? along (area.getX()) : along (area.getBottom());
    const juce::Point<float> end = horizontal ? along (area.getRight()) : along (area.getY());
    const auto stroke = juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.startNewSubPath (start);
    track.lineTo (end);
    g.setColour (slider.findColour (juce::Slider::backgroundColourId).withMultipliedAlpha (alpha));
    g.strokePath (track, stroke);

    // The value from the start, or from the centre when the range is centred on 0 (Output Pan).
    const bool bipolar = slider.getMinimum() < 0.0 && juce::approximatelyEqual (slider.getMinimum(), -slider.getMaximum());
    const auto thumb = along (position);
    juce::Path value;
    value.startNewSubPath (bipolar ? (start + end) / 2.0f : start);
    value.lineTo (thumb);
    g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (alpha));
    g.strokePath (value, stroke);

    const float thumbSize = 2.0f * static_cast<float> (getSliderThumbRadius (slider));
    const auto knob = juce::Rectangle<float> (thumbSize, thumbSize).withCentre (thumb);
    juce::Path dot;
    dot.addEllipse (knob);
    tokens::shadow::handle.drawForPath (g, dot);
    g.setColour (lit (slider.findColour (juce::Slider::thumbColourId), slider.isMouseOverOrDragging(), slider.isMouseButtonDown())
                     .withMultipliedAlpha (alpha));
    g.fillPath (dot);
}

int LookAndFeel::getSliderThumbRadius (juce::Slider&)
{
    return 6;
}

//==============================================================================
void LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));
    const float alpha = enabledAlpha (label);
    if (! label.isBeingEdited())
    {
        const auto labelFont = getLabelFont (label);
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (labelFont);
        const auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                          std::max (1, static_cast<int> (static_cast<float> (textArea.getHeight()) / labelFont.getHeight())),
                          label.getMinimumHorizontalScale());
    }
    g.setColour (label.findColour (label.isBeingEdited() ? juce::Label::outlineWhenEditingColourId : juce::Label::outlineColourId)
                     .withMultipliedAlpha (alpha));
    g.drawRect (label.getLocalBounds());
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<int> (width, height).toFloat(), size::r2);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;
    const bool focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<int> (width, height).toFloat().reduced (0.5f), size::r2, 1.0f);
}

//==============================================================================
int LookAndFeel::getDefaultScrollbarWidth()
{
    return 10;
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height, bool vertical,
                                 int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
{
    // A slim thumb, no track.
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto thumb = vertical ? area.withY (static_cast<float> (thumbStart)).withHeight (static_cast<float> (thumbSize)).reduced (2.0f, 1.0f)
                                : area.withX (static_cast<float> (thumbStart)).withWidth (static_cast<float> (thumbSize)).reduced (1.0f, 2.0f);
    g.setColour (lit (scrollbar.findColour (juce::ScrollBar::thumbColourId), isMouseOver, isMouseDown));
    g.fillRoundedRectangle (thumb, std::min (thumb.getWidth(), thumb.getHeight()) / 2.0f);
}

//==============================================================================
juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> screenPosition,
                                                    juce::Rectangle<int> parentArea)
{
    juce::AttributedString string;
    string.append (text, font (size::fs3), findColour (juce::TooltipWindow::textColourId));
    juce::TextLayout layout;
    layout.createLayout (string, 400.0f);
    const int width = static_cast<int> (std::ceil (layout.getWidth())) + 20;
    const int height = static_cast<int> (std::ceil (layout.getHeight())) + 12;
    return juce::Rectangle<int> (screenPosition.x > parentArea.getCentreX() ? screenPosition.x - (width + 12) : screenPosition.x + 24,
                                 screenPosition.y > parentArea.getCentreY() ? screenPosition.y - (height + 6) : screenPosition.y + 6,
                                 width, height)
        .constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (bounds, size::r2);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), size::r2, 1.0f);
    juce::AttributedString string;
    string.setJustification (juce::Justification::centredLeft);
    string.append (text, font (size::fs3), findColour (juce::TooltipWindow::textColourId));
    juce::TextLayout layout;
    layout.createLayout (string, bounds.getWidth() - 20.0f);
    layout.draw (g, bounds.reduced (10.0f, 6.0f));
}

//==============================================================================
std::unique_ptr<juce::FocusOutline> LookAndFeel::createFocusOutlineForComponent (juce::Component&)
{
    struct Ring final : public juce::FocusOutline::OutlineWindowProperties
    {
        juce::Rectangle<int> getOutlineBounds (juce::Component& component) override
        {
            return component.getScreenBounds().expanded (static_cast<int> (size::focusOffset + size::focusWidth));
        }

        void drawOutline (juce::Graphics& g, int width, int height) override
        {
            const auto ring = juce::Rectangle<int> (width, height).toFloat().reduced (size::focusWidth / 2.0f);
            g.setColour (colour::focus);
            g.drawRoundedRectangle (ring, size::r2 + size::focusOffset + size::focusWidth / 2.0f, size::focusWidth);
        }
    };
    return std::make_unique<juce::FocusOutline> (std::make_unique<Ring>());
}

} // namespace staple
