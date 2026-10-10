#include "FooterBar.h"

#include "Accessibility.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "UiScale.h"
#include "staple/Fonts.h"
#include "staple/controls/Overlay.h"

#include "eq1/Response.h"

#include <cmath>
#include <limits>

namespace eq1
{

namespace
{
// A footer panel shown in a call-out while it is open. The call-out hides itself as it closes, and is
// deleted later: the panel goes back to the footer, hidden, as soon as it hides.
class Lent final : public juce::Component, private juce::ComponentListener
{
public:
    Lent (juce::Component& p, juce::Component& h) : panel (&p), home (&h)
    {
        setSize (p.getWidth(), p.getHeight());
        p.setTopLeftPosition (0, 0);
        addAndMakeVisible (p);
    }

    ~Lent() override
    {
        if (callOut != nullptr)
            callOut->removeComponentListener (this);
        giveBack();
    }

    // Once in the call-out, follows it.
    void parentHierarchyChanged() override
    {
        if (callOut == nullptr && getParentComponent() != nullptr)
        {
            callOut = getParentComponent();
            callOut->addComponentListener (this);
        }
    }

private:
    void componentVisibilityChanged (juce::Component& component) override
    {
        if (! component.isVisible())
            giveBack();
    }

    void giveBack()
    {
        if (panel != nullptr && home != nullptr && panel->getParentComponent() == this)
        {
            panel->setVisible (false);
            home->addChildComponent (*panel);
        }
    }

    juce::Component::SafePointer<juce::Component> panel, home, callOut;
};
} // namespace

// The Analyzer's settings: Pre, Post, Sidechain and Peak Hold, its Range, Speed and Resolution, and
// Analyzer Tilt. Follows settings restored with the plugin's state.
class FooterBar::AnalyzerPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AnalyzerPanel (PluginProcessor& p) : processor (p)
    {
        for (auto* toggle : { &showPreEq, &showPostEq, &showSidechain, &peakHold })
        {
            toggle->onClick = [this] { store(); };
            addAndMakeVisible (*toggle);
        }
        for (int range : { 60, 90, 120 })
            rangeMenu.addItem (juce::String (range) + " dB", range);
        speedMenu.addItemList ({ "Very Slow", "Slow", "Medium", "Fast", "Very Fast" }, 1);
        resolutionMenu.addItemList ({ "Low", "Medium", "High", "Maximum" }, 1);
        rangeMenu.setName ("Analyzer Range");
        speedMenu.setName ("Analyzer Speed");
        resolutionMenu.setName ("Analyzer Resolution");
        for (auto* combo : { &rangeMenu, &speedMenu, &resolutionMenu })
        {
            combo->onChange = [this] { store(); };
            addAndMakeVisible (*combo);
        }
        tiltLabel.setText ("Analyzer Tilt", juce::dontSendNotification);
        tiltLabel.setAccessible (false); // the slider carries the name
        addAndMakeVisible (tiltLabel);
        tilt.setSliderStyle (juce::Slider::LinearHorizontal);
        // In 0.5 dB/oct steps (the Staple Analyzer popover, #84, cycles Off, 3, 4.5 and 6): an arrow key
        // moves it one step, as a step smaller than the interval would round back.
        tilt.setRange (0.0, 6.0, 0.5);
        tilt.setTextValueSuffix (" dB/oct");
        tilt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 20);
        tilt.onValueChange = [this] { store(); };
        addAndMakeVisible (tilt);
        // What a screen reader calls each control, in the glossary's terms.
        const std::pair<juce::Component*, const char*> titles[] = {
            { &showPreEq, "Analyzer Pre-EQ" },     { &showPostEq, "Analyzer Post-EQ" },     { &showSidechain, "Analyzer Sidechain" },
            { &peakHold, "Analyzer Peak Hold" },   { &rangeMenu, "Analyzer Range" },        { &speedMenu, "Analyzer Speed" },
            { &resolutionMenu, "Analyzer Resolution" }, { &tilt, "Analyzer Tilt" }
        };
        for (auto [control, title] : titles)
            control->setTitle (title);
        show();
        startTimerHz (4);
        setSize (340, 3 * rowHeight + 2 * gap + 2 * padding);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (padding);
        auto toggles = area.removeFromTop (rowHeight);
        showPreEq.setBounds (toggles.removeFromLeft (56));
        showPostEq.setBounds (toggles.removeFromLeft (60));
        showSidechain.setBounds (toggles.removeFromLeft (90));
        peakHold.setBounds (toggles.removeFromLeft (84));
        area.removeFromTop (gap);
        auto combos = area.removeFromTop (rowHeight);
        for (auto* combo : { &rangeMenu, &speedMenu, &resolutionMenu })
        {
            combo->setBounds (combos.removeFromLeft (100));
            combos.removeFromLeft (gap);
        }
        area.removeFromTop (gap);
        auto tiltRow = area.removeFromTop (rowHeight);
        tiltLabel.setBounds (tiltRow.removeFromLeft (80));
        tilt.setBounds (tiltRow);
    }

private:
    static constexpr int rowHeight = 24, gap = 6, padding = 6;

    void timerCallback() override { show(); }

    void show()
    {
        const auto settings = processor.analyzerSettings();
        showPreEq.setToggleState (settings.showPreEq, juce::dontSendNotification);
        showPostEq.setToggleState (settings.showPostEq, juce::dontSendNotification);
        showSidechain.setToggleState (settings.showSidechain, juce::dontSendNotification);
        peakHold.setToggleState (settings.peakHold, juce::dontSendNotification);
        rangeMenu.setSelectedId (settings.rangeDb, juce::dontSendNotification);
        speedMenu.setSelectedId (static_cast<int> (settings.speed) + 1, juce::dontSendNotification);
        resolutionMenu.setSelectedId (static_cast<int> (settings.resolution) + 1, juce::dontSendNotification);
        tilt.setValue (settings.tiltDbPerOctave, juce::dontSendNotification);
    }

    void store()
    {
        processor.setAnalyzerSettings ({ .showPreEq = showPreEq.getToggleState(),
                                         .showPostEq = showPostEq.getToggleState(),
                                         .showSidechain = showSidechain.getToggleState(),
                                         .rangeDb = rangeMenu.getSelectedId(),
                                         .speed = static_cast<AnalyzerSpeed> (speedMenu.getSelectedId() - 1),
                                         .resolution = static_cast<AnalyzerResolution> (resolutionMenu.getSelectedId() - 1),
                                         .tiltDbPerOctave = tilt.getValue(),
                                         .peakHold = peakHold.getToggleState() });
    }

    PluginProcessor& processor;
    juce::ToggleButton showPreEq { "Pre" }, showPostEq { "Post" }, showSidechain { "Sidechain" }, peakHold { "Peak Hold" };
    juce::ComboBox rangeMenu, speedMenu, resolutionMenu;
    juce::Label tiltLabel;
    KeyboardSlider tilt { "Analyzer Tilt" };
};

namespace
{
namespace colour = staple::tokens::colour;
namespace size = staple::tokens::size;

constexpr int readoutHeight = 28, gainScaleWidth = 54, outputWidth = 70, readoutPadding = 10;
constexpr int footerGap = 18, bypassedOverlap = 10;
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
    g.setColour ((notApplied ? colour::text4 : colour::text1).withMultipliedAlpha (isEnabled() ? 1.0f : staple::tokens::motion::disabledAlpha));
    g.drawText (text(), getLocalBounds().reduced (readoutPadding, 0), juce::Justification::centred, false);
}

void GainScaleReadout::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    drag = Drag { e.position.y, getValue(), e.mods.isShiftDown() };
    gesture.emplace (*this);
}

void GainScaleReadout::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag)
        return;
    // Shift pressed or released mid-drag carries on from where the value is.
    if (e.mods.isShiftDown() != drag->fine)
        drag = Drag { e.position.y, getValue(), e.mods.isShiftDown() };
    const double moved = (drag->startY - e.position.y) * (drag->fine ? 0.25 : 1.0);
    setValue (juce::jlimit (getMinimum(), getMaximum(), static_cast<double> (juce::roundToInt (drag->startValue + moved))), juce::sendNotificationSync);
}

void GainScaleReadout::mouseUp (const juce::MouseEvent&)
{
    drag.reset();
    gesture.reset();
}

void GainScaleReadout::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled())
        return;
    const ScopedDragNotification reset (*this);
    setValue (100.0, juce::sendNotificationSync);
}

OutputReadout::OutputReadout() : staple::TextChip (outputReadoutText (0.0), Look::plain)
{
    setName ("Output");
    setTitle ("Output");
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

std::unique_ptr<juce::AccessibilityHandler> OutputReadout::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::button, [this] { return getButtonText(); }, [this] { triggerClick(); });
}

UiScaleMenu::UiScaleMenu() : staple::IconButton ("UI Scale", staple::Icon::uiScale)
{
    setTitle ("UI Scale");
    setTooltip ("UI Scale");
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
    : processor (p), outputValues (parameters::OutputValues::of (p.parameterState())), analyzerPanel (std::make_unique<AnalyzerPanel> (p)), popover (p)
{
    auto& state = processor.parameterState();
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slotValues.push_back (parameters::SlotValues::of (state, slot));

    globalBypass.setClickingTogglesState (true);
    globalBypass.setOffLook (true);
    globalBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::globalBypassId, globalBypass);
    globalBypass.setTitle (state.getParameter (parameters::globalBypassId)->getName (100));
    globalBypass.setTooltip ("Global Bypass (Cmd/Ctrl+B)");
    globalBypass.onStateChange = [this] { showBypass(); };
    addAndMakeVisible (globalBypass);

    analyzer.setTitle ("Analyzer");
    analyzer.onClick = [this] { openCallOut (*analyzerPanel, analyzer); };
    addAndMakeVisible (analyzer);
    addChildComponent (*analyzerPanel);

    gainScaleAttachment = std::make_unique<SliderAttachment> (state, parameters::gainScaleId, gainScale);
    gainScale.describe (*state.getParameter (parameters::gainScaleId));
    addAndMakeVisible (gainScale);

    output.onClick = [this] {
        if (popover.isOpen())
            popover.close();
        else
        {
            popover.openFrom (output);
            output.setOpen (popover.isOpen());
        }
    };
    addAndMakeVisible (output);
    // Back in the footer, hidden, once closed.
    popover.onClose = [this] {
        output.setOpen (false);
        addChildComponent (popover);
    };
    popover.onMeterToggled = [this] {
        if (onMeterToggled != nullptr)
            onMeterToggled();
    };
    addChildComponent (popover);

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
    startTimerHz (30);
}

FooterBar::~FooterBar()
{
    stopTimer();
    popover.onClose = nullptr;
    // An open call-out outlives the footer, deleted by its own callback: close it and take it off the
    // editor now.
    if (callOut != nullptr)
    {
        callOut->exitModalState (0);
        callOut->setVisible (false);
        if (auto* parent = callOut->getParentComponent())
            parent->removeChildComponent (callOut);
    }
}

void FooterBar::openCallOut (juce::Component& panel, juce::Component& from)
{
    auto* parent = getParentComponent();
    if (parent == nullptr || panel.isShowing())
        return;
    callOut = &juce::CallOutBox::launchAsynchronously (std::make_unique<Lent> (panel, *this), parent->getLocalArea (this, from.getBounds()), parent);
}

void FooterBar::showUiScale (int percent)
{
    uiScale.show (percent);
}

void FooterBar::showMeterShown (bool shown)
{
    popover.showMeterShown (shown);
}

void FooterBar::toggleGlobalBypass()
{
    globalBypass.setToggleState (! globalBypass.getToggleState(), juce::sendNotificationSync);
}

void FooterBar::timerCallback()
{
    showBypass();
    showOutputLevel();
    if (bypassedShown && bypassedAlpha < 1.0f)
        repaint();
}

void FooterBar::showBypass()
{
    const bool on = globalBypass.getToggleState();
    if (on == bypassedShown)
        return;
    bypassedShown = on;
    bypassedSince = juce::Time::getMillisecondCounterHiRes();
    bypassedAlpha = 0.0f;
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

void FooterBar::paint (juce::Graphics& g)
{
    if (! bypassedShown)
        return;
    // "Bypassed" fades in beside Global Bypass over dur2.
    bypassedAlpha = staple::ease (static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - bypassedSince) / staple::tokens::motion::dur2Ms));
    const auto font = bypassedFont();
    const int width = juce::roundToInt (std::ceil (juce::GlyphArrangement::getStringWidth (font, bypassedText)));
    g.setFont (font);
    g.setColour (colour::stateOff.withMultipliedAlpha (bypassedAlpha));
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
        const int width = juce::roundToInt (std::ceil (juce::GlyphArrangement::getStringWidth (bypassedFont(), bypassedText)));
        row.removeFromLeft (width - bypassedOverlap + footerGap);
    }
    analyzer.setBounds (row.removeFromLeft (80));

    uiScale.setBounds (row.removeFromRight (readoutHeight));
    row.removeFromRight (footerGap);
    // The readouts, 2 apart, 4 in from the UI Scale menu's gap.
    row.removeFromRight (4);
    output.setBounds (row.removeFromRight (std::max (outputWidth, output.getIdealWidth() + 2 * (readoutPadding - 6))));
    row.removeFromRight (2);
    const int gainScaleText = juce::roundToInt (std::ceil (juce::GlyphArrangement::getStringWidth (readoutFont(), "200%")));
    gainScale.setBounds (row.removeFromRight (std::max (gainScaleWidth, gainScaleText + 2 * readoutPadding)));
}

} // namespace eq1
