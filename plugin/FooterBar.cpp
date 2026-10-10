#include "FooterBar.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "UiScale.h"

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
        addAndMakeVisible (tiltLabel);
        tilt.setSliderStyle (juce::Slider::LinearHorizontal);
        // In 0.5 dB/oct steps (the Staple Analyzer popover, #84, cycles Off, 3, 4.5 and 6): an arrow key
        // moves it one step, as a step smaller than the interval would round back.
        tilt.setRange (0.0, 6.0, 0.5);
        tilt.setTextValueSuffix (" dB/oct");
        tilt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 20);
        tilt.onValueChange = [this] { store(); };
        addAndMakeVisible (tilt);
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

FooterBar::FooterBar (PluginProcessor& p) : processor (p), analyzerPanel (std::make_unique<AnalyzerPanel> (p)), outputPanel (p)
{
    auto& state = processor.parameterState();
    globalBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::globalBypassId, globalBypass);
    addAndMakeVisible (globalBypass);

    gainScale.setSliderStyle (juce::Slider::LinearHorizontal);
    gainScale.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    gainScaleAttachment = std::make_unique<SliderAttachment> (state, parameters::gainScaleId, gainScale);
    gainScaleLabel.setText ("Gain Scale", juce::dontSendNotification);
    gainScaleLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gainScaleLabel);
    addAndMakeVisible (gainScale);

    analyzer.setTitle ("Analyzer");
    output.setTitle ("Output");
    analyzer.onClick = [this] { openCallOut (*analyzerPanel, analyzer); };
    output.onClick = [this] { openCallOut (outputPanel, output); };
    addAndMakeVisible (analyzer);
    addAndMakeVisible (output);
    addChildComponent (*analyzerPanel);
    addChildComponent (outputPanel);

    showMeter.setTooltip ("Show the Output Meter");
    showMeter.setToggleState (processor.isOutputMeterShown(), juce::dontSendNotification);
    showMeter.onClick = [this] {
        processor.setOutputMeterShown (showMeter.getToggleState());
        if (onMeterToggled != nullptr)
            onMeterToggled();
    };
    addAndMakeVisible (showMeter);

    uiScale.setName ("UI Scale");
    uiScale.setTitle ("UI Scale");
    uiScale.setTooltip ("UI Scale");
    for (int percent : uiScale::percents)
        uiScale.addItem (juce::String (percent) + "%", percent);
    uiScale.onChange = [this] {
        if (onUiScalePicked != nullptr)
            onUiScalePicked (uiScale.getSelectedId());
    };
    addAndMakeVisible (uiScale);

    // On-screen order.
    int order = 0;
    for (juce::Component* child : std::initializer_list<juce::Component*> { &globalBypass, &analyzer, &gainScale, &output, &showMeter, &uiScale })
        child->setExplicitFocusOrder (++order);
}

FooterBar::~FooterBar()
{
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
    uiScale.setSelectedId (percent, juce::dontSendNotification);
}

void FooterBar::showMeterShown (bool shown)
{
    showMeter.setToggleState (shown, juce::dontSendNotification);
}

void FooterBar::resized()
{
    constexpr int controlHeight = 24, gap = 18;
    // Padded 0 0 0 6.
    auto row = getLocalBounds().withTrimmedLeft (6);
    row = row.withSizeKeepingCentre (row.getWidth(), controlHeight);
    uiScale.setBounds (row.removeFromRight (72));
    row.removeFromRight (gap);
    showMeter.setBounds (row.removeFromRight (64));
    row.removeFromRight (gap);

    globalBypass.setBounds (row.removeFromLeft (104));
    row.removeFromLeft (gap);
    analyzer.setBounds (row.removeFromLeft (80));
    row.removeFromLeft (gap);
    gainScaleLabel.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (6);
    // Up to 220 wide, leaving room for Output.
    gainScale.setBounds (row.removeFromLeft (juce::jlimit (60, 220, row.getWidth() - 70 - gap)));
    row.removeFromLeft (gap);
    output.setBounds (row.removeFromLeft (70));
}

} // namespace eq1
