#include "OutputPopover.h"

#include "Accessibility.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/controls/Overlay.h"

#include <cmath>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;
namespace motion = staple::tokens::motion;

constexpr int cardWidth = 176, paddingTop = 14, paddingSide = 12, paddingBottom = 10, gap = 12;
constexpr int knobDiameter = 64, panHeight = 14, panLabelGap = 4, panLabelHeight = 12, toggleHeight = 30, toggleGap = 4;
constexpr int chipHeight = 20, chipPadding = 6;
constexpr int cardHeight = paddingTop + knobDiameter + gap + panHeight + panLabelGap + panLabelHeight + gap + 1 + gap + toggleHeight + paddingBottom;

// The thumb's small shadow.
const juce::DropShadow thumbShadow { colour::shadow.withAlpha (0.5f), 3, { 0, 1 } };

float dimmed (const juce::Component& c) { return c.isEnabled() ? 1.0f : motion::disabledAlpha; }
} // namespace

juce::String outputPanText (double pan, bool midSide)
{
    const int percent = juce::roundToInt (pan);
    if (percent == 0)
        return "Centre";
    return juce::String (std::abs (percent)) + (percent < 0 ? (midSide ? " M" : " L") : (midSide ? " S" : " R"));
}

// Pan Mode's chip: "L/R" or "M/S"; a click switches.
class OutputPopover::PanModeChip final : public juce::Button
{
public:
    PanModeChip() : juce::Button ("Pan Mode")
    {
        setClickingTogglesState (true);
        setHasFocusOutline (true);
        setTitle ("Pan Mode");
        buttonStateChanged();
    }

    void buttonStateChanged() override
    {
        const bool midSide = getToggleState();
        setButtonText (midSide ? "M/S" : "L/R");
        setTooltip (midSide ? "Pan Mode M/S: Output Pan balances Mid against Side. Click for L/R"
                            : "Pan Mode L/R: Output Pan balances Left against Right. Click for M/S");
    }

    int idealWidth() const
    {
        return juce::roundToInt (std::ceil (juce::GlyphArrangement::getStringWidth (font(), "M/S"))) + 2 * chipPadding;
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const float alpha = dimmed (*this);
        const float light = down ? motion::pressedBrightness : highlighted ? motion::hoverBrightness : 1.0f;
        g.setColour (colour::fill1.withMultipliedAlpha (light * alpha));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), size::r1);
        g.setFont (font());
        g.setColour ((highlighted || down ? colour::text1 : colour::text2).withMultipliedAlpha (alpha));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }

private:
    static juce::Font font() { return staple::font (size::fs1, staple::Weight::semiBold); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return getButtonText(); }, [this] { triggerClick(); });
    }
};

OutputPanSlider::OutputPanSlider() : KeyboardSlider ("Output Pan")
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setArrowSteps (5.0, 1.0);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setTooltip ("Output Pan: drag; double-click to centre");
}

void OutputPanSlider::paint (juce::Graphics& g)
{
    const float alpha = dimmed (*this);
    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth();
    const float proportion = static_cast<float> (juce::jlimit (0.0, 1.0, (getValue() - getMinimum()) / (getMaximum() - getMinimum())));
    const float centreX = width * 0.5f, valueX = width * proportion;

    g.setColour (colour::fill2.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle ({ 0.0f, 4.0f, width, 6.0f }, 3.0f);
    g.setColour (colour::line3.withMultipliedAlpha (alpha));
    g.fillRect (juce::Rectangle<float> (centreX - 0.5f, 2.0f, 1.0f, 10.0f));
    g.setColour (colour::text2.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle ({ std::min (centreX, valueX), 4.0f, std::abs (valueX - centreX), 6.0f }, 3.0f);

    const juce::Rectangle<float> thumb { juce::jlimit (0.0f, width - 6.0f, valueX - 3.0f), 1.0f, 6.0f, 12.0f };
    staple::drawSoftShadow (g, thumb, 2.0f, thumbShadow);
    g.setColour (colour::text1.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (thumb, 2.0f);
}

void OutputPanSlider::setFromX (float x)
{
    const double proportion = juce::jlimit (0.0, 1.0, static_cast<double> (x) / std::max (1, getWidth()));
    setValue (std::round (getMinimum() + proportion * (getMaximum() - getMinimum())), juce::sendNotificationSync);
}

void OutputPanSlider::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    drag.emplace (*this);
    setFromX (e.position.x);
}

void OutputPanSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (drag)
        setFromX (e.position.x);
}

void OutputPanSlider::mouseUp (const juce::MouseEvent&)
{
    drag.reset();
}

void OutputPanSlider::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled())
        return;
    const ScopedDragNotification centre (*this);
    setValue (0.0, juce::sendNotificationSync);
}

OutputToggle::OutputToggle (const juce::String& title, std::optional<staple::Icon> i, const juce::String& letter)
    : juce::Button (title), icon (i)
{
    setClickingTogglesState (true);
    setHasFocusOutline (true);
    setTitle (title);
    setButtonText (letter);
}

void OutputToggle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const bool on = getToggleState();
    const auto bounds = getLocalBounds().toFloat();
    const float light = down ? motion::pressedBrightness : highlighted ? motion::hoverBrightness : 1.0f;
    if (on)
    {
        g.setColour (colour::fill2.withMultipliedAlpha (light));
        g.fillRoundedRectangle (bounds, size::r2);
    }
    const auto ink = (on || highlighted || down ? colour::text1 : colour::text3).withMultipliedAlpha (dimmed (*this));
    if (icon)
        staple::drawIcon (g, *icon, bounds.withSizeKeepingCentre (15.0f, 15.0f), ink);
    else
    {
        g.setFont (staple::font (size::fs4, staple::Weight::semiBold));
        g.setColour (ink);
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }
}

OutputPopover::OutputPopover (PluginProcessor& p)
    : processor (p), outputGain (static_cast<float> (knobDiameter), "Output Gain"), panMode (std::make_unique<PanModeChip>()),
      phaseInvert ("Phase Invert", staple::Icon::phaseInvert), autoGain ("Auto Gain", std::nullopt, "A"),
      showMeter ("Output Meter", staple::Icon::meter)
{
    auto& state = processor.parameterState();
    outputGain.setArcColour (colour::curveMain);
    outputGain.setBipolar (true);
    outputGainAttachment = std::make_unique<SliderAttachment> (state, parameters::outputGainId, outputGain);
    outputGain.describe (*state.getParameter (parameters::outputGainId));
    outputPanAttachment = std::make_unique<SliderAttachment> (state, parameters::outputPanId, outputPan);
    outputPan.describe (*state.getParameter (parameters::outputPanId));
    panModeAttachment = std::make_unique<ButtonAttachment> (state, parameters::panModeId, *panMode);
    phaseInvertAttachment = std::make_unique<ButtonAttachment> (state, parameters::phaseInvertId, phaseInvert);
    autoGainAttachment = std::make_unique<ButtonAttachment> (state, parameters::autoGainId, autoGain);
    panMode->setTitle (state.getParameter (parameters::panModeId)->getName (100));
    phaseInvert.setTitle (state.getParameter (parameters::phaseInvertId)->getName (100));
    autoGain.setTitle (state.getParameter (parameters::autoGainId)->getName (100));

    phaseInvert.setTooltip ("Phase Invert");
    autoGain.setTooltip ("Auto Gain: compensates the output level so EQ changes are heard without a loudness bias");
    showMeter.setTooltip ("Show or hide the Output Meter");
    showMeter.setToggleState (processor.isOutputMeterShown(), juce::dontSendNotification);
    showMeter.onClick = [this] {
        processor.setOutputMeterShown (showMeter.getToggleState());
        if (onMeterToggled != nullptr)
            onMeterToggled();
    };
    // A repaint for its readout as Output Pan or Pan Mode changes.
    outputPan.onValueChange = [this] { repaint (panLabelsArea()); };
    panMode->onStateChange = [this] { repaint (panLabelsArea()); };

    int order = 0;
    for (juce::Component* control : std::initializer_list<juce::Component*> { &outputGain, panMode.get(), &outputPan, &phaseInvert, &autoGain, &showMeter })
    {
        control->setExplicitFocusOrder (++order);
        addAndMakeVisible (*control);
    }

    setCardSize (cardWidth, cardHeight);
    followLayout();
    layoutCheck.startTimerHz (4);
}

OutputPopover::~OutputPopover() = default;

void OutputPopover::openFrom (juce::Component& readout)
{
    open (readout, Placement::above);
    if (auto* layer = getParentComponent(); layer != nullptr && isOpen())
    {
        const auto anchor = layer->getLocalArea (&readout, readout.getLocalBounds());
        setTopLeftPosition (std::max (-getCardBounds().getX(), anchor.getRight() - getCardBounds().getRight()), getY());
    }
}

void OutputPopover::showMeterShown (bool shown)
{
    showMeter.setToggleState (shown, juce::dontSendNotification);
}

juce::String OutputPopover::panReadout() const
{
    return outputPanText (outputPan.getValue(), panMode->getToggleState());
}

void OutputPopover::followLayout()
{
    // The track can change between mono and stereo while the editor is open.
    const bool stereo = processor.isOutputPanAvailable();
    if (outputPan.isEnabled() != stereo || panMode->isEnabled() != stereo)
    {
        outputPan.setEnabled (stereo);
        panMode->setEnabled (stereo);
        repaint();
    }
}

juce::Rectangle<int> OutputPopover::panLabelsArea() const
{
    return outputPan.getBounds().withY (outputPan.getBottom() + panLabelGap).withHeight (panLabelHeight);
}

void OutputPopover::paint (juce::Graphics& g)
{
    staple::Popover::paint (g);
    // Output Pan's ends and readout.
    const bool midSide = panMode->getToggleState();
    const auto labels = panLabelsArea();
    g.setFont (staple::font (size::fs1));
    g.setColour (colour::text3.withMultipliedAlpha (dimmed (outputPan)));
    g.drawText (midSide ? "Mid" : "L", labels, juce::Justification::centredLeft, false);
    g.drawText (panReadout(), labels, juce::Justification::centred, false);
    g.drawText (midSide ? "Side" : "R", labels, juce::Justification::centredRight, false);

    // The line between Output Pan and the toggles.
    g.setColour (colour::line2);
    g.fillRect (labels.getX(), labels.getBottom() + gap, labels.getWidth(), 1);
}

void OutputPopover::resized()
{
    auto area = getCardBounds().withTrimmedTop (paddingTop).withTrimmedBottom (paddingBottom).reduced (paddingSide, 0);
    auto knobRow = area.removeFromTop (knobDiameter);
    const int knobSize = outputGain.getIdealSize();
    // The face centred in the row, its shadow below.
    outputGain.setSize (knobSize, knobSize);
    outputGain.setTopLeftPosition (knobRow.getCentre() - outputGain.getFaceCentre().roundToInt());
    panMode->setBounds (knobRow.removeFromBottom (chipHeight).removeFromRight (panMode->idealWidth()));
    area.removeFromTop (gap);
    outputPan.setBounds (area.removeFromTop (panHeight));
    area.removeFromTop (panLabelGap + panLabelHeight + gap + 1 + gap);
    auto toggles = area.removeFromTop (toggleHeight);
    const int toggleWidth = (toggles.getWidth() - 2 * toggleGap) / 3;
    for (auto* toggle : { &phaseInvert, &autoGain, &showMeter })
    {
        toggle->setBounds (toggles.removeFromLeft (toggleWidth));
        toggles.removeFromLeft (toggleGap);
    }
}

} // namespace eq1
