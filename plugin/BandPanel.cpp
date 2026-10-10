#include "BandPanel.h"

#include "BandEditing.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/Tokens.h"

namespace eq1
{

namespace
{
// Threshold's slider runs from -60 dB to 0 dB, then one more step at the top for Auto.
constexpr double thresholdAutoPosition = 3.0;
// Positions above 0 dB are Auto. The slider's 0.1 dB steps put 0 dB a rounding error away from 0.
bool isAuto (double position) { return position > 0.05; }
} // namespace


std::array<std::pair<juce::Slider*, juce::Label*>, 10> BandPanel::rotaries()
{
    return { { { &frequency, &frequencyLabel },
               { &gain, &gainLabel },
               { &q, &qLabel },
               { &slope, &slopeLabel },
               { &dynamicRange, &dynamicRangeLabel },
               { &threshold, &thresholdLabel },
               { &attack, &attackLabel },
               { &release, &releaseLabel },
               { &detectionLow, &detectionLowLabel },
               { &detectionHigh, &detectionHighLabel } } };
}

BandPanel::BandPanel (PluginProcessor& p, BandEditing& e) : processor (p), editing (e)
{
    title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (title);

    shape.addItemList (parameters::shapeNames(), 1);
    placement.addItemList (parameters::placementNames(), 1);
    detectionSource.addItemList (parameters::detectionSourceNames(), 1);
    detectionRange.addItemList (parameters::detectionRangeNames(), 1);
    shape.setName ("Shape");
    placement.setName ("Stereo Placement");
    detectionSource.setName ("Detection Source");
    detectionRange.setName ("Detection Range");
    for (auto* combo : { &shape, &placement, &detectionSource, &detectionRange })
        addAndMakeVisible (*combo);

    const auto controls = rotaries();
    const char* names[] = { "Frequency", "Gain", "Q", "Slope", "Dynamic Range", "Threshold", "Attack", "Release", "Detection Low", "Detection High" };
    static_assert (std::size (names) == std::tuple_size_v<decltype (controls)>);
    for (size_t i = 0; i < std::size (controls); ++i)
    {
        auto [slider, label] = controls[i];
        slider->setName (names[i]);
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        label->setText (names[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        // The knob is titled with its parameter's name, so a screen reader doesn't stop at the label too.
        label->setAccessible (false);
        addAndMakeVisible (*slider);
        addAndMakeVisible (*label);
    }

    threshold.setRange (-60.0, thresholdAutoPosition, 0.1);
    threshold.textFromValueFunction = [] (double value) {
        return isAuto (value) ? juce::String ("Auto") : juce::String (value, 1) + " dB";
    };
    threshold.valueFromTextFunction = [] (const juce::String& text) {
        return text.trim().equalsIgnoreCase ("Auto") ? thresholdAutoPosition : juce::jmin (0.0, text.getDoubleValue());
    };
    // Between 0 dB and Auto there are no values: a step up from 0 dB is Auto, a step down from Auto 0 dB.
    threshold.landStep = [] (double from, double to) {
        if (isAuto (from))
            return to < from ? 0.0 : from;
        return isAuto (to) ? thresholdAutoPosition : to;
    };
    threshold.onDragStart = [this] {
        thresholdDragging = true;
        thresholdAttachment->beginGesture();
        thresholdAutoAttachment->beginGesture();
    };
    threshold.onDragEnd = [this] {
        thresholdAttachment->endGesture();
        thresholdAutoAttachment->endGesture();
        thresholdDragging = false;
    };
    threshold.onValueChange = [this] { storeThreshold(); };
    addAndMakeVisible (detectionArc);

    for (auto* button : { &brickwall, &bypass, &dynamicsBypass })
        addAndMakeVisible (*button);
    deleteButton.onClick = [this] {
        if (slot != 0)
            editing.deleteBand (slot);
    };
    addAndMakeVisible (deleteButton);
    // Detection Audition lasts while the button is held.
    audition.onStateChange = [this] {
        if (audition.isDown() && slot != 0)
            processor.setDetectionAudition (slot);
        else
            releaseAudition();
    };
    addAndMakeVisible (audition);

    // Tab's order: the top row, the left column, then the knobs, each as laid out.
    int order = 0;
    for (juce::Component* control : std::initializer_list<juce::Component*> { &brickwall, &dynamicsBypass, &bypass, &audition, &deleteButton,
                                                                                &shape, &placement, &detectionSource, &detectionRange })
        control->setExplicitFocusOrder (++order);
    for (auto [slider, label] : controls)
        slider->setExplicitFocusOrder (++order);

    show (0);
    startTimerHz (10);
}

BandPanel::~BandPanel()
{
    releaseAudition();
    processor.setMeteredBand (0);
}

void BandPanel::releaseAudition()
{
    if (processor.detectionAuditionSlot() != 0)
        processor.setDetectionAudition (0);
}

void BandPanel::show (int newSlot)
{
    releaseAudition();
    slot = newSlot;
    // Attachments are rebuilt for the new slot; the old ones go first so they let go of the controls.
    shapeAttachment.reset();
    placementAttachment.reset();
    detectionSourceAttachment.reset();
    detectionRangeAttachment.reset();
    detectionLowAttachment.reset();
    detectionHighAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    slopeAttachment.reset();
    brickwallAttachment.reset();
    bypassAttachment.reset();
    dynamicRangeAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    dynamicsBypassAttachment.reset();
    thresholdAttachment.reset();
    thresholdAutoAttachment.reset();

    for (int i = 0; i < getNumChildComponents(); ++i)
        getChildComponent (i)->setVisible (slot != 0 || getChildComponent (i) == &title);
    if (slot == 0)
    {
        processor.setMeteredBand (0);
        title.setText ("Double-click the display to add a Band", juce::dontSendNotification);
        return;
    }

    auto& state = processor.parameterState();
    title.setText ("Band " + juce::String (slot), juce::dontSendNotification);
    shapeAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::shapeId (slot), shape);
    placementAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::placementId (slot), placement);
    frequencyAttachment = std::make_unique<SliderAttachment> (state, parameters::frequencyId (slot), frequency);
    gainAttachment = std::make_unique<SliderAttachment> (state, parameters::gainId (slot), gain);
    qAttachment = std::make_unique<SliderAttachment> (state, parameters::qId (slot), q);
    slopeAttachment = std::make_unique<SliderAttachment> (state, parameters::slopeId (slot), slope);
    brickwallAttachment = std::make_unique<ButtonAttachment> (state, parameters::brickwallId (slot), brickwall);
    bypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::bypassId (slot), bypass);
    dynamicRangeAttachment = std::make_unique<SliderAttachment> (state, parameters::dynamicRangeId (slot), dynamicRange);
    attackAttachment = std::make_unique<SliderAttachment> (state, parameters::attackId (slot), attack);
    releaseAttachment = std::make_unique<SliderAttachment> (state, parameters::releaseId (slot), release);
    dynamicsBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::dynamicsBypassId (slot), dynamicsBypass);
    detectionSourceAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::detectionSourceId (slot), detectionSource);
    detectionRangeAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::detectionRangeId (slot), detectionRange);
    detectionLowAttachment = std::make_unique<SliderAttachment> (state, parameters::detectionLowId (slot), detectionLow);
    detectionHighAttachment = std::make_unique<SliderAttachment> (state, parameters::detectionHighId (slot), detectionHigh);
    thresholdAttachment = std::make_unique<juce::ParameterAttachment> (*state.getParameter (parameters::thresholdId (slot)),
                                                                       [this] (float) { showThreshold(); });
    thresholdAutoAttachment = std::make_unique<juce::ParameterAttachment> (*state.getParameter (parameters::thresholdAutoId (slot)),
                                                                           [this] (float) { showThreshold(); });
    showThreshold();
    describe();
    updateVisibility();
}

void BandPanel::describe()
{
    auto& state = processor.parameterState();
    const auto name = [&state] (const juce::String& id) { return state.getParameter (id)->getName (100); };
    const std::pair<KeyboardSlider*, juce::String> sliders[] = { { &frequency, parameters::frequencyId (slot) },
                                                                 { &gain, parameters::gainId (slot) },
                                                                 { &q, parameters::qId (slot) },
                                                                 { &slope, parameters::slopeId (slot) },
                                                                 { &dynamicRange, parameters::dynamicRangeId (slot) },
                                                                 { &attack, parameters::attackId (slot) },
                                                                 { &release, parameters::releaseId (slot) },
                                                                 { &detectionLow, parameters::detectionLowId (slot) },
                                                                 { &detectionHigh, parameters::detectionHighId (slot) } };
    for (const auto& [slider, id] : sliders)
        slider->describe (*state.getParameter (id));
    // Read as it shows: Auto at its top, else its dB.
    threshold.setTitle (name (parameters::thresholdId (slot)));
    const std::pair<juce::Component*, juce::String> others[] = { { &shape, parameters::shapeId (slot) },
                                                                  { &placement, parameters::placementId (slot) },
                                                                  { &detectionSource, parameters::detectionSourceId (slot) },
                                                                  { &detectionRange, parameters::detectionRangeId (slot) },
                                                                  { &brickwall, parameters::brickwallId (slot) },
                                                                  { &bypass, parameters::bypassId (slot) },
                                                                  { &dynamicsBypass, parameters::dynamicsBypassId (slot) } };
    for (const auto& [control, id] : others)
        control->setTitle (name (id));
    const auto band = "Band " + juce::String (slot) + " ";
    deleteButton.setTitle (band + "Delete");
    audition.setTitle (band + "Detection Audition");
}

void BandPanel::showThreshold()
{
    if (slot == 0)
        return;
    // From the parameters themselves: while their listeners are told of a change, the raw values that
    // BandEditing reads may not have caught up yet.
    auto& state = processor.parameterState();
    const auto& level = *state.getParameter (parameters::thresholdId (slot));
    const bool automatic = state.getParameter (parameters::thresholdAutoId (slot))->getValue() >= 0.5f;
    threshold.setValue (automatic ? thresholdAutoPosition : level.convertFrom0to1 (level.getValue()), juce::dontSendNotification);
}

void BandPanel::storeThreshold()
{
    if (slot == 0 || thresholdAttachment == nullptr)
        return;
    const double value = threshold.getValue();
    const bool automatic = isAuto (value);
    // A drag is one gesture on both parameters; a typed value is a gesture of its own.
    if (thresholdDragging)
    {
        thresholdAutoAttachment->setValueAsPartOfGesture (automatic ? 1.0f : 0.0f);
        if (! automatic)
            thresholdAttachment->setValueAsPartOfGesture (static_cast<float> (value));
    }
    else
    {
        // One undo step, though Auto and Threshold are each set as a gesture.
        processor.editHistory().beginTransaction();
        thresholdAutoAttachment->setValueAsCompleteGesture (automatic ? 1.0f : 0.0f);
        if (! automatic)
            thresholdAttachment->setValueAsCompleteGesture (static_cast<float> (value));
        processor.editHistory().endTransaction();
    }
    // The attachments don't call back for their own changes: a key step or a typed value shows what
    // was stored, such as Auto at its top position. A mouse drag carries on from where it is.
    if (! threshold.isMouseButtonDown())
        showThreshold();
}

void BandPanel::updateVisibility()
{
    if (slot == 0)
        return;
    const auto band = editing.band (slot);
    if (! band.inUse)
    {
        show (0);
        return;
    }
    for (auto* c : std::initializer_list<juce::Component*> { &gain, &gainLabel })
        c->setVisible (hasGain (band.shape));
    // Cut, Notch, Band Pass and All Pass keep their dynamics settings but don't offer them.
    for (auto* c : std::initializer_list<juce::Component*> { &dynamicRange, &dynamicRangeLabel, &threshold, &thresholdLabel, &detectionArc,
                                                             &attack, &attackLabel, &release, &releaseLabel, &dynamicsBypass,
                                                             &detectionSource, &detectionRange, &audition })
        c->setVisible (hasDynamics (band.shape));
    for (auto* c : std::initializer_list<juce::Component*> { &detectionLow, &detectionLowLabel, &detectionHigh, &detectionHighLabel })
        c->setVisible (hasDynamics (band.shape) && band.detectionRange == DetectionRange::Free);
    // A Shape without dynamics has no detection signal to audition or meter.
    if (! hasDynamics (band.shape))
        releaseAudition();
    processor.setMeteredBand (hasDynamics (band.shape) ? slot : 0);
    brickwall.setVisible (isCut (band.shape));
    // Brickwall overrides a Cut's Slope.
    const bool usesSlope = hasSlope (band.shape) && ! (isCut (band.shape) && band.brickwall);
    for (auto* c : std::initializer_list<juce::Component*> { &slope, &slopeLabel })
        c->setVisible (usesSlope);
    placement.setVisible (processor.isStereoPlacementAvailable());
}

void BandPanel::timerCallback()
{
    updateVisibility();
}

void BandPanel::paint (juce::Graphics& g)
{
    g.fillAll (staple::tokens::colour::raised);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto top = area.removeFromTop (26);
    title.setBounds (top.removeFromLeft (260));
    deleteButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (8);
    audition.setBounds (top.removeFromRight (140));
    top.removeFromRight (8);
    bypass.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);
    dynamicsBypass.setBounds (top.removeFromRight (140));
    top.removeFromRight (8);
    brickwall.setBounds (top.removeFromRight (100));
    area.removeFromTop (6);

    auto left = area.removeFromLeft (150);
    shape.setBounds (left.removeFromTop (24));
    left.removeFromTop (8);
    placement.setBounds (left.removeFromTop (24));
    left.removeFromTop (8);
    detectionSource.setBounds (left.removeFromTop (24));
    left.removeFromTop (8);
    detectionRange.setBounds (left.removeFromTop (24));

    const auto controls = rotaries();
    const int width = area.getWidth() / static_cast<int> (std::size (controls));
    for (auto [slider, label] : controls)
    {
        auto column = area.removeFromLeft (width);
        label->setBounds (column.removeFromTop (16));
        slider->setBounds (column);
    }
    detectionArc.setBounds (threshold.getBounds());
}

} // namespace eq1
