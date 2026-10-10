#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "staple/Tokens.h"
#include "staple/controls/Overlay.h"

#include "eq1/Engine.h"

namespace eq1
{

namespace layout = staple::tokens::layout;

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), eqProcessor (p), editing (p.parameterState(), p.editHistory()), display (p, editing), panel (p, editing), header (p),
      footer (p), meter (p), keyboard (p.editHistory())
{
    display.onSelectionChanged = [this] (int slot) { panel.show (slot); };
    panel.onSelectBand = [this] (int slot) { display.selectBand (slot); };
    // Popovers and knob tooltips float in it, scaled with everything else.
    staple::markOverlayLayer (content);
    content.addAndMakeVisible (display);
    content.addChildComponent (meter);
    // Over the display, so its clicks never reach it.
    content.addChildComponent (panel);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (footer);

    // The display's Gain range, saved with the plugin.
    for (int range : { 6, 12, 30 })
        displayRange.addItem ("+/- " + juce::String (range) + " dB", range);
    displayRange.setName ("Display Range");
    displayRange.setTitle ("Display Range");
    displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    displayRange.onChange = [this] {
        eqProcessor.setDisplayRangeDb (displayRange.getSelectedId());
        display.repaint();
    };
    content.addAndMakeVisible (displayRange);
    content.addChildComponent (header.presets().browserPanel());

    footer.onUiScalePicked = [this] (int percent) {
        eqProcessor.pickUiScale (percent);
        applyUiScale();
    };
    footer.onMeterToggled = [this] { resized(); };

    // Tab's order: the header (and the Preset browser while open), Display Range, the display and its
    // Bands, the Output Meter, the Band panel and the footer.
    int order = 0;
    for (juce::Component* child : std::initializer_list<juce::Component*> { &header, &header.presets().browserPanel(), &displayRange, &display,
                                                                             &meter, &panel, &footer })
        child->setExplicitFocusOrder (++order);

    startTimerHz (4);

    // Once every component is in place: each one that keeps something from its look (a Slider's
    // text box, a ComboBox's label colours) takes it again from this.
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    keyboard.adopt (*this);
    addMouseListener (this, true);

    setResizable (true, true);
    applyUiScale();
}

void PluginEditor::applyUiScale()
{
    // In logical pixels: below the minimum the Band panel would collide with the frequency labels.
    constexpr int minimumWidth = layout::minimumWidth, minimumHeight = layout::minimumHeight, maximumWidth = 2560, maximumHeight = 1600;
    const auto stored = eqProcessor.editorSize();
    const juce::Point<int> size { juce::jlimit (minimumWidth, maximumWidth, stored.x), juce::jlimit (minimumHeight, maximumHeight, stored.y) };
    const int percent = eqProcessor.uiScalePercent();
    footer.showUiScale (percent);
    shownScalePercent = percent;
    scale = static_cast<float> (percent) / 100.0f;
    content.setTransform (juce::AffineTransform::scale (scale));
    const auto scaled = [this] (int logical) { return juce::roundToInt (static_cast<float> (logical) * scale); };
    // New limits can resize the window on the way to its size: keep that from the processor.
    applyingScale = true;
    setResizeLimits (scaled (minimumWidth), scaled (minimumHeight), scaled (maximumWidth), scaled (maximumHeight));
    setSize (scaled (size.x), scaled (size.y));
    applyingScale = false;
    resized();
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::mouseDown (const juce::MouseEvent&)
{
    lookAndFeel.showFocusRing (false);
}

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::tabKey) && ! isParentOf (getCurrentlyFocusedComponent()))
    {
        const auto all = createKeyboardFocusTraverser()->getAllComponents (this);
        if (all.empty())
            return false;
        lookAndFeel.showFocusRing (true);
        (key.getModifiers().isShiftDown() ? all.back() : all.front())->grabKeyboardFocus();
        return true;
    }
    const auto command = juce::ModifierKeys::commandModifier;
    if (key == juce::KeyPress ('b', command, 0))
    {
        footer.toggleGlobalBypass();
        return true;
    }
    if (key == juce::KeyPress ('z', command, 0))
    {
        header.undo();
        return true;
    }
    if (key == juce::KeyPress ('z', command | juce::ModifierKeys::shiftModifier, 0) || key == juce::KeyPress ('y', command, 0))
    {
        header.redo();
        return true;
    }
    return false;
}

void PluginEditor::timerCallback()
{
    header.showUndoState();
    // Follows settings restored with the plugin's state.
    if (displayRange.getSelectedId() != eqProcessor.displayRangeDb())
        displayRange.setName ("Display Range");
    displayRange.setSelectedId (eqProcessor.displayRangeDb(), juce::dontSendNotification);
    if (meter.isVisible() != eqProcessor.isOutputMeterShown())
    {
        footer.showMeterShown (eqProcessor.isOutputMeterShown());
        resized();
    }
    if (eqProcessor.uiScalePercent() != shownScalePercent || eqProcessor.editorSize() != shownSize)
        applyUiScale();
}

namespace
{
// A soft elliptical highlight over the window: colour at centre (a proportion of the window's size),
// fading to nothing at fadeOut of the radii, as CSS's radial-gradient (rx ry at x y, colour, transparent fadeOut).
void paintHighlight (juce::Graphics& g, juce::Rectangle<float> window, juce::Point<float> at, float rx, float ry, juce::Colour colour, float fadeOut)
{
    const juce::Point<float> centre { window.getWidth() * at.x, window.getHeight() * at.y };
    const juce::Graphics::ScopedSaveState saved (g);
    // A circle of radius rx, squashed to ry vertically.
    const auto squash = juce::AffineTransform::scale (1.0f, ry / rx, centre.x, centre.y);
    g.addTransform (squash);
    g.setGradientFill (juce::ColourGradient (colour, centre, colour.withAlpha (0.0f), centre.translated (rx * fadeOut, 0.0f), true));
    g.fillRect (window.transformedBy (squash.inverted()));
}
} // namespace

void PluginEditor::paint (juce::Graphics& g)
{
    namespace colour = staple::tokens::colour;
    g.fillAll (colour::bg0);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto window = content.getLocalBounds().toFloat();
    paintHighlight (g, window, { 0.12f, 0.04f }, 700.0f, 480.0f, colour::windowHighlight1, 0.70f);
    paintHighlight (g, window, { 0.92f, 0.32f }, 800.0f, 560.0f, colour::windowHighlight2, 0.66f);
    paintHighlight (g, window, { 0.45f, 1.12f }, 720.0f, 480.0f, colour::windowHighlight3, 0.66f);
}

void PluginEditor::resized()
{
    const juce::Point<int> logical { juce::roundToInt (static_cast<float> (getWidth()) / scale), juce::roundToInt (static_cast<float> (getHeight()) / scale) };
    if (! applyingScale)
    {
        eqProcessor.setEditorSize (logical);
        shownSize = eqProcessor.editorSize();
    }
    content.setBounds (0, 0, logical.x, logical.y);

    // Padded all round, with a gap between the rows; the display runs flush to the window's left edge.
    auto area = content.getLocalBounds().reduced (layout::outerPadding);
    header.setBounds (area.removeFromTop (layout::headerHeight));
    area.removeFromTop (layout::gap);
    footer.setBounds (area.removeFromBottom (layout::footerHeight));
    area.removeFromBottom (layout::gap);
    area.setLeft (0);
    meter.setVisible (eqProcessor.isOutputMeterShown());
    if (meter.isVisible())
    {
        meter.setBounds (area.removeFromRight (layout::meterWidth));
        area.removeFromRight (layout::gap);
    }
    display.setBounds (area);

    // Over the display: Display Range at its top right, the Band panel centred along its bottom, and
    // the Preset browser.
    constexpr int displayRangeWidth = 110, displayRangeHeight = 24;
    displayRange.setBounds (area.getRight() - 6 - displayRangeWidth, area.getY() + 8, displayRangeWidth, displayRangeHeight);
    panel.setBounds (area.getCentreX() - BandPanel::width / 2, area.getBottom() - layout::bandPanelAboveBottom - BandPanel::height,
                     BandPanel::width, BandPanel::height);
    header.presets().browserPanel().setBounds (area.reduced (40, 12));
}

} // namespace eq1
