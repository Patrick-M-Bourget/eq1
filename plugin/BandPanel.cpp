#include "BandPanel.h"

#include "BandEditing.h"
#include "Parameters.h"
#include "PluginProcessor.h"

namespace eq1
{

BandPanel::BandPanel (PluginProcessor& p, BandEditing& e) : processor (p), editing (e)
{
    title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (title);

    shape.addItemList (parameters::shapeNames(), 1);
    placement.addItemList (parameters::placementNames(), 1);
    for (auto* combo : { &shape, &placement })
        addAndMakeVisible (*combo);

    const std::pair<juce::Slider*, juce::Label*> controls[] = {
        { &frequency, &frequencyLabel }, { &gain, &gainLabel }, { &q, &qLabel }, { &slope, &slopeLabel }
    };
    const char* names[] = { "Frequency", "Gain", "Q", "Slope" };
    for (size_t i = 0; i < std::size (controls); ++i)
    {
        auto [slider, label] = controls[i];
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        label->setText (names[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*slider);
        addAndMakeVisible (*label);
    }

    for (auto* button : { &brickwall, &bypass })
        addAndMakeVisible (*button);
    deleteButton.onClick = [this] {
        if (slot != 0)
            editing.deleteBand (slot);
    };
    addAndMakeVisible (deleteButton);

    show (0);
    startTimerHz (10);
}

BandPanel::~BandPanel() = default;

void BandPanel::show (int newSlot)
{
    slot = newSlot;
    // Attachments are rebuilt for the new slot; the old ones go first so they let go of the controls.
    shapeAttachment.reset();
    placementAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    slopeAttachment.reset();
    brickwallAttachment.reset();
    bypassAttachment.reset();

    for (int i = 0; i < getNumChildComponents(); ++i)
        getChildComponent (i)->setVisible (slot != 0 || getChildComponent (i) == &title);
    if (slot == 0)
    {
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
    updateVisibility();
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
    g.fillAll (juce::Colour (0xff1d2027));
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto top = area.removeFromTop (26);
    title.setBounds (top.removeFromLeft (260));
    deleteButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (8);
    bypass.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);
    brickwall.setBounds (top.removeFromRight (100));
    area.removeFromTop (6);

    auto left = area.removeFromLeft (150);
    shape.setBounds (left.removeFromTop (24));
    left.removeFromTop (8);
    placement.setBounds (left.removeFromTop (24));

    const std::pair<juce::Slider*, juce::Label*> controls[] = {
        { &frequency, &frequencyLabel }, { &gain, &gainLabel }, { &q, &qLabel }, { &slope, &slopeLabel }
    };
    const int width = area.getWidth() / static_cast<int> (std::size (controls));
    for (auto [slider, label] : controls)
    {
        auto column = area.removeFromLeft (width);
        label->setBounds (column.removeFromTop (16));
        slider->setBounds (column);
    }
}

} // namespace eq1
