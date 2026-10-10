#include "FooterBar.h"

#include "Accessibility.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "UiScale.h"
#include "staple/Fonts.h"
#include "staple/Light.h"

#include "eq1/Response.h"

#include <cmath>
#include <limits>

namespace eq1
{

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr int readoutHeight = 28, gainScaleWidth = 54, outputWidth = 70, readoutPadding = 10;
constexpr int footerGap = 18, bypassedOverlap = 10, analyzerLabelGap = 8;
const juce::String bypassedText { "Bypassed" };

// Whether Auto Gain's estimate holds for both: the same Bands, exactly, and Gain Scale. (Settings' own ==
// would do, but its float comparison warns where it is defined.)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
bool sameEstimate (const Settings& a, const Settings& b)
{
    for (size_t i = 0; i < a.bands.size(); ++i)
    {
        const auto &x = a.bands[i], &y = b.bands[i];
        if (x.inUse != y.inUse || x.bypass != y.bypass || x.shape != y.shape || x.frequency != y.frequency || x.gain != y.gain
            || x.q != y.q || x.slope != y.slope || x.brickwall != y.brickwall || x.placement != y.placement
            || x.dynamicRange != y.dynamicRange || x.threshold != y.threshold || x.thresholdAuto != y.thresholdAuto
            || x.attack != y.attack || x.release != y.release || x.dynamicsBypass != y.dynamicsBypass)
            return false;
    }
    return a.gainScale == b.gainScale;
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

juce::Font readoutFont() { return staple::font (size::fs3); }
juce::Font bypassedFont() { return staple::font (size::fs3, staple::Weight::medium); }
} // namespace

juce::String outputReadoutText (double db)
{
    if (! std::isfinite (db) || db <= parameters::outputGainSilentDb)
        return "-inf dB";
    const double rounded = std::round (db * 10.0) / 10.0;
    return (rounded >= 0.0 ? "+" : "-") + juce::String (std::abs (rounded), 1) + " dB";
}

GainScaleReadout::GainScaleReadout() : KeyboardSlider ("Gain Scale")
{
    setSliderStyle (juce::Slider::LinearBarVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setArrowSteps (5.0, 1.0);
    setDoubleClickReturnValue (true, 100.0);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Gain Scale: scales every Band's Gain and Dynamic Range. Drag up or down; double-click for 100%");
}

void GainScaleReadout::setNotApplied (bool n)
{
    notApplied = n;
    repaint();
}

juce::String GainScaleReadout::text() const { return juce::String (juce::roundToInt (getValue())) + "%"; }

void GainScaleReadout::paint (juce::Graphics& g)
{
    g.setFont (readoutFont());
    g.setColour ((notApplied ? colour::text4 : colour::text1).withMultipliedAlpha (staple::enabledAlpha (*this)));
    g.drawText (text(), getLocalBounds().reduced (readoutPadding, 0), juce::Justification::centred, false);
}

void GainScaleReadout::mouseDown (const juce::MouseEvent& e)
{
    if (isEnabled() && e.mods.isLeftButtonDown())
        startMouseDrag (e);
}

void GainScaleReadout::mouseDrag (const juce::MouseEvent& e)
{
    continueMouseDrag (e);
}

void GainScaleReadout::mouseUp (const juce::MouseEvent&)
{
    endMouseDrag();
}

double GainScaleReadout::valueDraggedBy (double from, float pixels, bool fine)
{
    return juce::jlimit (getMinimum(), getMaximum(), static_cast<double> (juce::roundToInt (from + pixels * (fine ? 0.25 : 1.0))));
}

OutputReadout::OutputReadout() : staple::TextChip (outputReadoutText (0.0), Look::plain)
{
    setName ("Output");
    setTitle ("Output");
    setTextIsValue (true);
    setTooltip ("Output: Output Gain, with Auto Gain's estimate while Auto Gain is on. Click for the output controls");
    setInk (colour::text1);
}

void OutputReadout::setNotApplied (bool n)
{
    notApplied = n;
    setInk (n ? colour::text4 : colour::text1);
}

void OutputReadout::setOpen (bool o)
{
    open = o;
    repaint();
}

void OutputReadout::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (open)
    {
        g.setColour (colour::fill2);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), size::r2);
    }
    staple::TextChip::paintButton (g, highlighted && ! open, down && ! open);
}

UiScaleMenu::UiScaleMenu() : staple::IconButton ("UI Scale", staple::Icon::uiScale)
{
    setTitle ("UI Scale");
    setTooltip ("UI Scale");
    setIconSize (14.0f);
    onClick = [this] {
        menu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withDeletionCheck (*this),
                              [safe = juce::Component::SafePointer<UiScaleMenu> (this)] (int picked) {
                                  if (safe != nullptr && picked > 0)
                                      safe->pick (picked);
                              });
    };
}

juce::PopupMenu UiScaleMenu::menu() const
{
    juce::PopupMenu items;
    items.addSectionHeader ("UI Scale");
    for (int percent : uiScale::percents)
        items.addItem (percent, juce::String (percent) + "%", true, percent == shown);
    return items;
}

void UiScaleMenu::pick (int percent)
{
    shown = percent;
    if (onPicked != nullptr)
        onPicked (percent);
}

std::unique_ptr<juce::AccessibilityHandler> UiScaleMenu::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return juce::String (shown) + "%"; }, [this] { triggerClick(); });
}

FooterBar::FooterBar (PluginProcessor& p)
    : processor (p), outputValues (parameters::OutputValues::of (p.parameterState())), analyzerPopover (p), outputPopover (p)
{
    bypassedFade.apply = [this] (float) { repaint(); };
    auto& state = processor.parameterState();
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slotValues.push_back (parameters::SlotValues::of (state, slot));

    globalBypass.setClickingTogglesState (true);
    globalBypass.setOffLook (true);
    globalBypass.setIconSize (14.0f);
    globalBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::globalBypassId, globalBypass);
    globalBypass.setTitle (state.getParameter (parameters::globalBypassId)->getName (100));
    globalBypass.setTooltip ("Global Bypass (Cmd/Ctrl+B)");
    globalBypass.onStateChange = [this] { showBypass(); };
    addAndMakeVisible (globalBypass);

    analyzerLabel.setText ("Analyzer", juce::dontSendNotification);
    analyzerLabel.setFont (staple::font (size::fs3));
    analyzerLabel.setColour (juce::Label::textColourId, colour::text2);
    analyzerLabel.setBorderSize ({});
    analyzerLabel.setInterceptsMouseClicks (false, false);
    analyzerLabel.setAccessible (false); // the button carries the name
    addAndMakeVisible (analyzerLabel);
    analyzer.onClick = [this] {
        if (analyzerPopover.isOpen())
            analyzerPopover.close();
        else
            analyzerPopover.openFrom (analyzer, analyzerLabel);
    };
    addAndMakeVisible (analyzer);
    // Back in the footer, hidden, once closed.
    analyzerPopover.onClose = [this] { addChildComponent (analyzerPopover); };
    analyzerPopover.onSettingsChanged = [this] { showAnalyzerSources(); };
    addChildComponent (analyzerPopover);

    gainScaleAttachment = std::make_unique<SliderAttachment> (state, parameters::gainScaleId, gainScale);
    gainScale.describe (*state.getParameter (parameters::gainScaleId));
    addAndMakeVisible (gainScale);

    output.onClick = [this] {
        if (outputPopover.isOpen())
            outputPopover.close();
        else
        {
            outputPopover.openFrom (output);
            output.setOpen (outputPopover.isOpen());
        }
    };
    addAndMakeVisible (output);
    // Back in the footer, hidden, once closed.
    outputPopover.onClose = [this] {
        output.setOpen (false);
        addChildComponent (outputPopover);
    };
    outputPopover.onMeterToggled = [this] {
        if (onMeterToggled != nullptr)
            onMeterToggled();
    };
    addChildComponent (outputPopover);

    uiScale.onPicked = [this] (int percent) {
        if (onUiScalePicked != nullptr)
            onUiScalePicked (percent);
    };
    addAndMakeVisible (uiScale);

    // On-screen order.
    int order = 0;
    for (juce::Component* child : std::initializer_list<juce::Component*> { &globalBypass, &analyzer, &gainScale, &output, &uiScale })
        child->setExplicitFocusOrder (++order);

    showBypass();
    showOutputLevel();
    showAnalyzerSources();
    startTimerHz (30);
}

FooterBar::~FooterBar()
{
    stopTimer();
    outputPopover.onClose = nullptr;
    analyzerPopover.onClose = nullptr;
}

void FooterBar::showUiScale (int percent)
{
    uiScale.show (percent);
}

void FooterBar::toggleGlobalBypass()
{
    globalBypass.setToggleState (! globalBypass.getToggleState(), juce::sendNotificationSync);
}

void FooterBar::timerCallback()
{
    showBypass();
    showOutputLevel();
    showAnalyzerSources();
}

void FooterBar::showBypass()
{
    const bool on = globalBypass.getToggleState();
    if (on == bypassedShown)
        return;
    bypassedShown = on;
    bypassedFade.jump (0.0f);
    if (on)
        bypassedFade.towards (1.0f);
    gainScale.setNotApplied (on);
    output.setNotApplied (on);
    resized();
    repaint();
}

void FooterBar::showOutputLevel()
{
    Settings settings;
    for (size_t slot = 0; slot < slotValues.size(); ++slot)
        settings.bands[slot] = slotValues[slot].read();
    outputValues.readInto (settings);
    double db = settings.outputGainDb;
    if (settings.autoGain && std::isfinite (db))
    {
        // Worked out again only when the Bands or Gain Scale change, at the rate the curve is drawn at.
        if (! estimated || ! sameEstimate (settings, estimatedFor))
        {
            const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
            estimateDb = autoGainDb (settings, sampleRate);
            estimatedFor = settings;
            estimated = true;
        }
        db += estimateDb;
    }
    const auto text = outputReadoutText (db);
    if (text != output.getButtonText())
    {
        output.setButtonText (text);
        resized();
    }
}

void FooterBar::showAnalyzerSources()
{
    const auto text = analyzerButtonText (processor.analyzerSettings());
    if (text != analyzer.getButtonText())
    {
        analyzer.setButtonText (text);
        resized();
    }
}

void FooterBar::paint (juce::Graphics& g)
{
    if (! bypassedShown)
        return;
    // "Bypassed" fades in beside Global Bypass over dur2.
    const auto font = bypassedFont();
    const int width = staple::textWidth (font, bypassedText);
    g.setFont (font);
    g.setColour (colour::stateOff.withMultipliedAlpha (bypassedFade.value()));
    g.drawText (bypassedText, globalBypass.getRight() + footerGap - bypassedOverlap, 0, width, getHeight(), juce::Justification::centredLeft, false);
}

void FooterBar::resized()
{
    // Padded 0 0 0 6.
    auto row = getLocalBounds().withTrimmedLeft (6);
    row = row.withSizeKeepingCentre (row.getWidth(), readoutHeight);

    globalBypass.setBounds (row.removeFromLeft (readoutHeight));
    row.removeFromLeft (footerGap);
    if (bypassedShown)
    {
        const int width = staple::textWidth (bypassedFont(), bypassedText);
        row.removeFromLeft (width - bypassedOverlap + footerGap);
    }
    // "Analyzer", then its button, 8 apart.
    const int labelWidth = staple::textWidth (analyzerLabel.getFont(), analyzerLabel.getText());
    analyzerLabel.setBounds (row.removeFromLeft (labelWidth));
    row.removeFromLeft (analyzerLabelGap);
    analyzer.setBounds (row.removeFromLeft (analyzer.getIdealWidth()));

    uiScale.setBounds (row.removeFromRight (readoutHeight));
    row.removeFromRight (footerGap);
    // The readouts, 2 apart, 4 in from the UI Scale menu's gap.
    row.removeFromRight (4);
    output.setBounds (row.removeFromRight (std::max (outputWidth, output.getIdealWidth() + 2 * (readoutPadding - 6))));
    row.removeFromRight (2);
    const int gainScaleText = staple::textWidth (readoutFont(), "200%");
    gainScale.setBounds (row.removeFromRight (std::max (gainScaleWidth, gainScaleText + 2 * readoutPadding)));
}

} // namespace eq1
