#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "staple/Tokens.h"
#include "staple/WindowBackground.h"
#include "staple/controls/Overlay.h"

#include "eq1/Engine.h"

namespace eq1
{

namespace layout = staple::tokens::layout;

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), eqProcessor (p), editing (p.parameterState(), p.editHistory()), display (p, editing), panel (p, editing), detectionRange (p, editing, panel), header (p),
      footer (p), displayRange (p), meter (p), keyboard (p.editHistory())
{
    display.onSelectionChanged = [this] (int slot) { panel.show (slot); };
    panel.onSelectBand = [this] (int slot) { display.selectBand (slot); };
    // Popovers and knob tooltips float in it, scaled with everything else.
    staple::markOverlayLayer (content);
    content.addAndMakeVisible (display);
    content.addChildComponent (meter);
    // Over the display, so its clicks never reach it, and over the Detection Range bar.
    content.addChildComponent (detectionRange);
    content.addChildComponent (panel);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (footer);

    // The display's Gain range, saved with the plugin; the display follows it on its timer.
    content.addAndMakeVisible (displayRange);
    content.addChildComponent (header.presetBrowser());

    footer.onUiScalePicked = [this] (int percent) {
        eqProcessor.pickUiScale (percent);
        applyUiScale();
    };
    footer.onMeterToggled = [this] { resized(); };

    // Tab's order: the header (and the Preset browser while open), Display Range, the display and its
    // Bands, the Output Meter, the Band panel, its Detection Range bar and the footer.
    int order = 0;
    for (juce::Component* child : std::initializer_list<juce::Component*> { &header, &header.presetBrowser(), &displayRange, &display,
                                                                             &meter, &panel, &detectionRange, &footer })
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
    // In logical pixels.
    using layout::minimumWidth, layout::minimumHeight, layout::maximumWidth, layout::maximumHeight;
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

void PluginEditor::mouseDown (const juce::MouseEvent& e)
{
    lookAndFeel.showFocusRing (false);
    display.pressedInEditor (e.eventComponent);
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
    if (meter.isVisible() != eqProcessor.isOutputMeterShown())
        resized();
    if (eqProcessor.uiScalePercent() != shownScalePercent || eqProcessor.editorSize() != shownSize)
        applyUiScale();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale));
    staple::paintWindowBackground (g, content.getLocalBounds().toFloat());
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

    // Over the display: Display Range at its top right and the Band panel centred along its bottom.
    constexpr int displayRangeRight = 6, displayRangeTop = 8, displayRangeHeight = 24;
    const int displayRangeWidth = displayRange.getIdealWidth();
    displayRange.setBounds (area.getRight() - displayRangeRight - displayRangeWidth, area.getY() + displayRangeTop, displayRangeWidth, displayRangeHeight);
    panel.setAnchor ({ area.getCentreX(), area.getBottom() - layout::bandPanelAboveBottom });
    detectionRange.setDisplayBounds (area);
    // The Preset browser, a modal over everything.
    header.presetBrowser().setBounds (content.getLocalBounds());
}

} // namespace eq1
