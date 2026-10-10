#include "BandPanel.h"

#include "BandEditing.h"
#include "BandMenu.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "staple/Fonts.h"
#include "staple/Tokens.h"
#include "staple/controls/KnobTooltip.h"
#include "staple/controls/Overlay.h"

#include <algorithm>
#include <cmath>

namespace eq1
{

namespace
{
namespace tokens = staple::tokens;
namespace colour = tokens::colour;
namespace layout = tokens::layout;

// The layout, in the panel's coordinates at 100 % (HANDOFF.md §4, prototype Main.dc.html): the bell
// rises bandPanelBell above the slab, whose content starts bandPanelPaddingTop below the slab's top.
constexpr int bell = layout::bandPanelBell;
constexpr int contentTop = bell + layout::bandPanelPaddingTop;
constexpr int labelHeight = 15, labelGap = 2;
constexpr int contentHeight = static_cast<int> (tokens::knob::gain) + labelGap + labelHeight; // the Gain column
constexpr int columnHeight = layout::edgeSelectorHeight + 8 + 22;                                 // Shape, Slope
constexpr int columnGap = 10, knobGap = 8;
constexpr int frequencyColumn = 64, gainColumn = 98, qColumn = 64;
constexpr int rowHeight = 22;
static_assert (BandPanel::height == contentTop + contentHeight + layout::bandPanelPaddingBottom);
static_assert (BandPanel::width
               == 2 * layout::edgeSelectorWidth + 4 * columnGap + 2 + frequencyColumn + gainColumn + qColumn + 2 * knobGap);
static_assert (BandPanel::openWidth == BandPanel::width + layout::dynamicsSectionWidth + knobGap);

// The bell's spread, as a proportion of the width, and the panel's Bypassed opacity.
constexpr float bellSigma = 0.14f;
constexpr float bypassedAlpha = 0.38f;
// The wash of the Band's colour from the top centre, and the hairline along the top edge.
constexpr float washTop = 0.14f, washMid = 0.035f, hairlineAlpha = 0.45f;

staple::Icon iconOf (Shape shape)
{
    using staple::Icon;
    switch (shape)
    {
        case Shape::Bell: return Icon::bell;
        case Shape::LowShelf: return Icon::lowShelf;
        case Shape::LowCut: return Icon::lowCut;
        case Shape::HighShelf: return Icon::highShelf;
        case Shape::HighCut: return Icon::highCut;
        case Shape::Notch: return Icon::notch;
        case Shape::BandPass: return Icon::bandPass;
        case Shape::TiltShelf: return Icon::tiltShelf;
        case Shape::FlatTilt: return Icon::flatTilt;
        case Shape::AllPass: return Icon::allPass;
    }
    return Icon::bell;
}

// Each Stereo Placement's icon, the side it leaves alone (drawn under it at 30 %) and its dot.
struct PlacementLook
{
    staple::Icon icon;
    std::optional<staple::Icon> context;
    juce::Colour dot;
};

PlacementLook lookOf (StereoPlacement placement)
{
    using staple::Icon;
    switch (placement)
    {
        case StereoPlacement::Stereo: return { Icon::placementStereo, std::nullopt, colour::placeStereo };
        case StereoPlacement::Left: return { Icon::placementLeft, Icon::placementRight, colour::placeLeft };
        case StereoPlacement::Right: return { Icon::placementRight, Icon::placementLeft, colour::placeRight };
        case StereoPlacement::Mid: return { Icon::placementMid, Icon::placementStereo, colour::placeMid };
        case StereoPlacement::Side: return { Icon::placementSide, Icon::placementStereo, colour::placeSide };
    }
    return { Icon::placementStereo, std::nullopt, colour::placeStereo };
}
} // namespace

//==============================================================================
BandPanel::SlopeButton::SlopeButton (BandPanel& p) : staple::Knob (tokens::knob::small, "Slope"), panel (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::String BandPanel::SlopeButton::text()
{
    if (panel.slot != 0)
        if (const auto band = panel.editing.band (panel.slot); isCut (band.shape) && band.brickwall)
            return "Brickwall";
    const double value = getValue();
    const bool whole = std::abs (value - std::round (value)) < 0.05;
    return juce::String (value, whole ? 0 : 1) + " dB/oct";
}

void BandPanel::SlopeButton::paint (juce::Graphics& g)
{
    if (! isEnabled())
        g.beginTransparencyLayer (tokens::motion::disabledAlpha);
    const auto bounds = getLocalBounds().toFloat();
    if (isMouseOverOrDragging() && isEnabled())
    {
        g.setColour (colour::fill1);
        g.fillRoundedRectangle (bounds, tokens::size::r2);
    }
    g.setFont (staple::font (tokens::size::fs3, staple::Weight::medium));
    g.setColour (colour::text1);
    g.drawText (text(), bounds, juce::Justification::centred, false);
    if (! isEnabled())
        g.endTransparencyLayer();
}

bool BandPanel::SlopeButton::hitTest (int, int) { return true; }

void BandPanel::SlopeButton::mouseEnter (const juce::MouseEvent&) { repaint(); }

void BandPanel::SlopeButton::mouseExit (const juce::MouseEvent& e)
{
    repaint();
    staple::Knob::mouseExit (e);
}

void BandPanel::SlopeButton::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    pressY = e.position.y;
    startProportion = valueToProportionOfLength (getValue());
    fine = e.mods.isShiftDown();
}

void BandPanel::SlopeButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! e.mods.isLeftButtonDown())
        return;
    if (! dragging)
    {
        if (e.getDistanceFromDragStart() <= dragThreshold)
            return;
        dragging = true;
        gesture.emplace (*this);
    }
    // Shift pressed or let go mid-drag carries on from where it is, at the new speed.
    if (e.mods.isShiftDown() != fine)
    {
        fine = e.mods.isShiftDown();
        pressY = e.position.y;
        startProportion = valueToProportionOfLength (getValue());
    }
    const float pixels = fine ? tokens::knob::fineDragPixels : tokens::knob::dragPixels;
    setValue (proportionOfLengthToValue (juce::jlimit (0.0, 1.0, startProportion + (pressY - e.position.y) / pixels)), juce::sendNotificationSync);
}

void BandPanel::SlopeButton::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        gesture.reset();
        return;
    }
    // A double-click's second release leaves the list its first one opened.
    if (isEnabled() && ! e.mods.isPopupMenu() && e.getNumberOfClicks() == 1 && ! e.mouseWasDraggedSinceMouseDown())
        openList();
}

bool BandPanel::SlopeButton::keyPressed (const juce::KeyPress& key)
{
    if (! isEnabled() || (key != juce::KeyPress::spaceKey && key != juce::KeyPress::returnKey))
        return staple::Knob::keyPressed (key);
    openList();
    return true;
}

juce::PopupMenu BandPanel::SlopeButton::list()
{
    auto menu = slopeMenu (panel.editing, { panel.slot });
    menu.addSeparator();
    menu.addItem (juce::String::fromUTF8 ("Type a value\xe2\x80\xa6"), isEnabled(), false, [safe = juce::Component::SafePointer<SlopeButton> (this)] {
        if (safe == nullptr)
            return;
        safe->showTooltip();
        if (auto* tooltip = safe->getKnobTooltip())
            tooltip->startEditing();
    });
    return menu;
}

void BandPanel::SlopeButton::openList()
{
    if (panel.slot == 0 || ! isShowing() || ! isEnabled())
        return;
    list().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()));
}

void BandPanel::SlopeButton::valueChanged()
{
    staple::Knob::valueChanged();
    repaint();
}

void BandPanel::SlopeButton::startedDragging()
{
    if (panel.slot == 0 || ! isCut (panel.editing.band (panel.slot).shape) || ! panel.editing.band (panel.slot).brickwall)
        return;
    brickwall = panel.processor.parameterState().getParameter (parameters::brickwallId (panel.slot));
    brickwall->beginChangeGesture();
    brickwall->setValueNotifyingHost (0.0f);
}

void BandPanel::SlopeButton::stoppedDragging()
{
    if (brickwall != nullptr)
        brickwall->endChangeGesture();
    brickwall = nullptr;
}

//==============================================================================
void BandPanel::Tween::towards (float target)
{
    if (juce::exactlyEqual (target, to))
        return;
    from = now;
    to = target;
    startedMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void BandPanel::Tween::jump (float target)
{
    from = to = now = target;
    stopTimer();
    if (apply != nullptr)
        apply (now);
}

void BandPanel::Tween::timerCallback()
{
    const auto elapsed = static_cast<float> ((juce::Time::getMillisecondCounterHiRes() - startedMs) / durationMs);
    now = from + (to - from) * staple::ease (elapsed);
    if (elapsed >= 1.0f)
    {
        now = to;
        stopTimer();
    }
    if (apply != nullptr)
        apply (now);
}

//==============================================================================
BandPanel::BandPanel (PluginProcessor& p, BandEditing& e) : processor (p), editing (e), ring (p, gain), section (p)
{
    slope = std::make_unique<SlopeButton> (*this);

    bypass.setClickingTogglesState (true);
    bypass.setOffLook (true);
    bypass.setRestColour (colour::text2);
    bypass.setIconSize (14.0f);
    // Solo lasts while the button is held, by the mouse or by Space; it lights while it lasts.
    solo.setClickingTogglesState (false);
    solo.setToggleable (true);
    solo.setIconSize (13.0f);
    solo.onStateChange = [this] {
        if (solo.isDown() && slot != 0)
        {
            processor.holdSolo (slot, PluginProcessor::SoloHolder::panel);
            solo.setToggleState (true, juce::dontSendNotification);
        }
        else if (! solo.isDown())
        {
            releaseSolo();
        }
    };
    for (auto* chevron : { &previous, &next })
        chevron->setIconSize (11.0f);
    previous.setTitle ("Previous Band");
    next.setTitle ("Next Band");
    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    deleteButton.setIconSize (12.0f);
    deleteButton.onClick = [this] {
        if (slot == 0)
            return;
        releaseSolo();
        editing.deleteBand (slot);
    };
    for (auto* button : { &bypass, &solo, &previous, &next, &deleteButton })
        addAndMakeVisible (*button);

    for (int i = 0; i < parameters::shapeNames().size(); ++i)
        shape.addItem (parameters::shapeNames()[i], i + 1, iconOf (static_cast<Shape> (i)));
    for (int i = 0; i < parameters::placementNames().size(); ++i)
    {
        const auto look = lookOf (static_cast<StereoPlacement> (i));
        placement.addItem (parameters::placementNames()[i], i + 1, look.icon, look.dot, look.context);
    }
    addAndMakeVisible (shape);
    addAndMakeVisible (placement);
    addAndMakeVisible (*slope);

    gain.setBipolar (true);
    const std::pair<juce::Label*, const char*> labels[] = { { &frequencyLabel, "Frequency" }, { &gainLabel, "Gain" }, { &qLabel, "Q" } };
    for (auto [label, text] : labels)
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (staple::font (tokens::size::fs2));
        label->setColour (juce::Label::textColourId, colour::text3);
        label->setJustificationType (juce::Justification::centredTop);
        label->setBorderSize ({});
        // The knob is titled with its parameter's name, so a screen reader doesn't stop at the label too.
        label->setAccessible (false);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }
    for (auto* knob : { &frequency, &gain, &q })
        addAndMakeVisible (*knob);

    addAndMakeVisible (ring);

    // The dynamics icons above Gain, and the section they open.
    clearDynamics.setIconSize (10.0f);
    clearDynamics.onClick = [this] {
        if (slot != 0)
            editing.clearDynamics ({ slot });
    };
    dynamicsBypass.setClickingTogglesState (true);
    dynamicsBypass.setOffLook (true);
    dynamicsBypass.setRestColour (colour::text2);
    dynamicsBypass.setIconSize (12.0f);
    dynamicsOpen.setIconSize (11.0f);
    dynamicsOpen.onClick = [this] {
        sectionWanted = ! sectionWanted;
        showDynamics (true);
    };
    int iconOrder = 0;
    for (auto* button : { &clearDynamics, &dynamicsBypass, &dynamicsOpen })
    {
        button->setExplicitFocusOrder (++iconOrder);
        dynamicsIcons.addAndMakeVisible (*button);
    }
    dynamicsIcons.setInterceptsMouseClicks (false, true);
    addChildComponent (dynamicsIcons);
    addChildComponent (section);

    // Tab's order: the top row, the left column, the knobs with the dynamics icons over Gain, its ring
    // and the dynamics section after it, then the right column, each as laid out.
    int order = 0;
    for (juce::Component* control : std::initializer_list<juce::Component*> { &bypass, &solo, &previous, &next, &deleteButton, &shape,
                                                                                slope.get(), &frequency, &dynamicsIcons, &gain, &ring, &section,
                                                                                &q, &placement })
        control->setExplicitFocusOrder (++order);

    fade.apply = [this] (float alpha) {
        for (auto* c : faded())
            c->setAlpha (alpha);
        repaint();
    };

    slide.apply = [this] (float) {
        placeAtWidth();
        if (slide.now <= 0.0f && slide.to <= 0.0f)
            section.setVisible (false);
    };

    show (0);
    startTimerHz (10);
}

BandPanel::~BandPanel()
{
    releaseSolo();
    section.releaseAudition();
    processor.setMeteredBand (0);
}

void BandPanel::releaseSolo()
{
    solo.setToggleState (false, juce::dontSendNotification);
    processor.releaseSolo (PluginProcessor::SoloHolder::panel);
}

std::vector<juce::Component*> BandPanel::faded()
{
    return { &solo,           &previous,       &next,         &shape,   slope.get(), &frequency, &gain,          &q,
             &frequencyLabel, &gainLabel,      &qLabel,       &placement, &clearDynamics, &dynamicsBypass, &dynamicsOpen, &section };
}

juce::Colour BandPanel::bandColour() const
{
    return tokens::band[static_cast<size_t> (std::max (1, slot) - 1)];
}

void BandPanel::show (int newSlot)
{
    releaseSolo();
    slot = newSlot;
    // Attachments are rebuilt for the new slot; the old ones go first so they let go of the controls.
    shapeAttachment.reset();
    placementAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    slopeAttachment.reset();
    bypassAttachment.reset();
    dynamicsBypassAttachment.reset();
    ring.show (0);
    section.show (0);

    setVisible (slot != 0);
    if (slot == 0)
    {
        processor.setMeteredBand (0);
        return;
    }

    auto& state = processor.parameterState();
    shapeAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::shapeId (slot), shape);
    placementAttachment = std::make_unique<ComboBoxAttachment> (state, parameters::placementId (slot), placement);
    frequencyAttachment = std::make_unique<SliderAttachment> (state, parameters::frequencyId (slot), frequency);
    gainAttachment = std::make_unique<SliderAttachment> (state, parameters::gainId (slot), gain);
    qAttachment = std::make_unique<SliderAttachment> (state, parameters::qId (slot), q);
    slopeAttachment = std::make_unique<SliderAttachment> (state, parameters::slopeId (slot), *slope);
    bypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::bypassId (slot), bypass);
    dynamicsBypassAttachment = std::make_unique<ButtonAttachment> (state, parameters::dynamicsBypassId (slot), dynamicsBypass);
    ring.show (slot);
    section.show (slot);

    const auto band = bandColour();
    shape.setEdgeColour (band);
    shape.setIconColour (band);
    placement.setEdgeColour (band);
    solo.setLitColour (band);
    for (auto* knob : { &frequency, &gain, &q })
        knob->setArcColour (band);
    section.setBandColour (band);
    describe();
    // A Band shown afresh shows its Bypassed state and its dynamics at once.
    const bool bypassed = editing.band (slot).bypass;
    fade.jump (bypassed ? bypassedAlpha : 1.0f);
    updateAvailability();
    showDynamics (false);
    resized();
    repaint();
}

void BandPanel::describe()
{
    auto& state = processor.parameterState();
    const auto name = [&state] (const juce::String& id) { return state.getParameter (id)->getName (100); };
    const std::pair<KeyboardSlider*, juce::String> sliders[] = { { &frequency, parameters::frequencyId (slot) },
                                                                 { &gain, parameters::gainId (slot) },
                                                                 { &q, parameters::qId (slot) },
                                                                 { slope.get(), parameters::slopeId (slot) } };
    for (const auto& [slider, id] : sliders)
        slider->describe (*state.getParameter (id));
    // Read as it shows: Brickwall on a Brickwall Cut.
    slope->spokenValue = [this, spoken = slope->spokenValue] (double value) {
        const auto text = slope->text();
        return text == "Brickwall" || spoken == nullptr ? text : spoken (value);
    };
    const std::pair<juce::Component*, juce::String> others[] = { { &shape, parameters::shapeId (slot) },
                                                                  { &placement, parameters::placementId (slot) },
                                                                  { &bypass, parameters::bypassId (slot) },
                                                                  { &dynamicsBypass, parameters::dynamicsBypassId (slot) } };
    for (const auto& [control, id] : others)
        control->setTitle (name (id));
    const auto band = "Band " + juce::String (slot) + " ";
    solo.setTitle (band + "Solo");
    deleteButton.setTitle (band + "Delete");
    clearDynamics.setTitle (band + "Clear Dynamics");
}

void BandPanel::updateAvailability()
{
    if (slot == 0)
        return;
    const auto band = editing.band (slot);
    if (! band.inUse)
    {
        show (0);
        return;
    }
    // Shown whatever the Shape, so nothing moves: dimmed and out of Tab's way where unavailable.
    const auto offer = [] (std::initializer_list<juce::Component*> controls, bool available) {
        for (auto* c : controls)
            c->setEnabled (available);
    };
    offer ({ &gain, &gainLabel }, hasGain (band.shape));
    offer ({ slope.get() }, hasSlope (band.shape));
    // Flat Tilt's design ignores Q.
    offer ({ &q, &qLabel }, band.shape != Shape::FlatTilt);
    offer ({ &placement }, processor.isStereoPlacementAvailable());
    // The ring goes with Gain: Cut, Notch, Band Pass and All Pass keep their dynamics settings but
    // don't offer them.
    ring.setAvailable (hasGain (band.shape));
    showDynamics (true);
    processor.setMeteredBand (hasDynamics (band.shape) ? slot : 0);
    fade.towards (band.bypass ? bypassedAlpha : 1.0f);
    slope->repaint();
}

void BandPanel::step (int direction)
{
    std::vector<std::pair<double, int>> inUse;
    for (int s = 1; s <= numBandSlots; ++s)
        if (const auto band = editing.band (s); band.inUse)
            inUse.push_back ({ band.frequency, s });
    if (inUse.empty())
        return;
    // By Frequency, the lower Band Slot first on a tie, as Tab reaches them on the display.
    std::stable_sort (inUse.begin(), inUse.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    const auto here = std::find_if (inUse.begin(), inUse.end(), [this] (const auto& b) { return b.second == slot; });
    const auto count = static_cast<int> (inUse.size());
    const int index = here == inUse.end() ? 0 : (static_cast<int> (here - inUse.begin()) + direction + count) % count;
    const int target = inUse[static_cast<size_t> (index)].second;
    if (onSelectBand != nullptr)
        onSelectBand (target);
    else
        show (target);
}

void BandPanel::showDynamics (bool animate)
{
    if (slot == 0)
        return;
    const bool dynamic = isDynamic (editing.band (slot));
    if (dynamic && ! dynamicsIcons.isVisible())
        juce::Desktop::getInstance().getAnimator().fadeIn (&dynamicsIcons, tokens::motion::dur2Ms);
    else if (! dynamic)
        dynamicsIcons.setVisible (false);

    const bool open = sectionWanted && dynamic;
    // A section that isn't there has no detection signal to audition.
    if (! open)
        section.releaseAudition();
    const auto band = "Band " + juce::String (slot) + " dynamics";
    dynamicsOpen.setTitle ((sectionWanted ? "Hide " : "Show ") + band);
    dynamicsOpen.setIconRotation (sectionWanted ? juce::MathConstants<float>::pi : 0.0f);
    dynamicsOpen.setRestColour (sectionWanted ? colour::text1 : colour::text3);
    if (open)
        section.setVisible (true);
    if (animate)
        slide.towards (open ? 1.0f : 0.0f);
    else
        slide.jump (open ? 1.0f : 0.0f);
}

bool BandPanel::isDynamicsOpen() const { return slide.to > 0.0f; }

void BandPanel::setAnchor (juce::Point<int> bottomCentre)
{
    anchor = bottomCentre;
    placeAtWidth();
}

void BandPanel::placeAtWidth()
{
    const int w = width + juce::roundToInt (slide.now * static_cast<float> (openWidth - width));
    if (anchor.has_value())
        setBounds (anchor->x - w / 2, anchor->y - height, w, height);
    else
        setSize (w, height);
}

void BandPanel::timerCallback()
{
    updateAvailability();
}

//==============================================================================
bool BandPanel::hitTest (int x, int y)
{
    const auto p = juce::Point<int> (x, y);
    return slab.contains (p.toFloat()) || (dynamicsIcons.isVisible() && dynamicsIcons.getBounds().contains (p))
           || (section.isVisible() && section.getBounds().contains (p));
}

void BandPanel::resized()
{
    const auto w = static_cast<float> (getWidth());
    const float slabTop = bell, r = tokens::size::r3, sigma = bellSigma * w;
    const auto bellAt = [&] (float x) { return slabTop - static_cast<float> (bell) * std::exp (-(x - w / 2.0f) * (x - w / 2.0f) / (2.0f * sigma * sigma)); };
    // The top edge, from the left corner's end to the right one's start, along the bell.
    slab.clear();
    slab.startNewSubPath (0.0f, slabTop + r);
    slab.quadraticTo (0.0f, slabTop, r, slabTop);
    for (float x = r; x <= w - r; x += 2.0f)
        slab.lineTo (x, bellAt (x));
    slab.lineTo (w - r, slabTop);
    slab.quadraticTo (w, slabTop, w, slabTop + r);
    slab.lineTo (w, static_cast<float> (getHeight()) - r);
    slab.quadraticTo (w, static_cast<float> (getHeight()), w - r, static_cast<float> (getHeight()));
    slab.lineTo (r, static_cast<float> (getHeight()));
    slab.quadraticTo (0.0f, static_cast<float> (getHeight()), 0.0f, static_cast<float> (getHeight()) - r);
    slab.closeSubPath();

    // The top row.
    const int button = layout::iconButton;
    bypass.setBounds (8, bell + 6, button, button);
    solo.setBounds (36, bell + 6, button, button);
    deleteButton.setBounds (getWidth() - 8 - button, bell + 6, button, button);
    const auto numberFont = staple::font (tokens::size::fs4, staple::Weight::semiBold);
    const int numberWidth = std::max (16, juce::roundToInt (std::ceil (juce::GlyphArrangement::getStringWidth (numberFont, juce::String (slot)))));
    auto selector = juce::Rectangle<int> (getWidth() - 34 - (2 * rowHeight + numberWidth), bell + 7, 2 * rowHeight + numberWidth, rowHeight);
    previous.setBounds (selector.removeFromLeft (rowHeight));
    next.setBounds (selector.removeFromRight (rowHeight));
    numberArea = selector;

    // The columns, centred down the content beside the knobs.
    const int columnTop = contentTop + (contentHeight - columnHeight) / 2;
    shape.setBounds (0, columnTop, layout::edgeSelectorWidth, layout::edgeSelectorHeight);
    slope->setBounds (layout::edgeSelectorWidth - layout::slopeButtonWidth, columnTop + layout::edgeSelectorHeight + 8, layout::slopeButtonWidth, rowHeight);
    placement.setBounds (getWidth() - layout::edgeSelectorWidth, columnTop, layout::edgeSelectorWidth, layout::edgeSelectorHeight);
    dividers[0] = layout::edgeSelectorWidth + columnGap;
    dividers[1] = getWidth() - layout::edgeSelectorWidth - columnGap - 1;

    // The knobs, centred on Gain's centre, with their labels on one baseline under them.
    int x = dividers[0] + 1 + columnGap;
    const int labelTop = contentTop + contentHeight - labelHeight;
    const float centreY = static_cast<float> (contentTop) + tokens::knob::gain / 2.0f;
    const std::tuple<staple::Knob*, juce::Label*, int> columns[] = { { &frequency, &frequencyLabel, frequencyColumn },
                                                                      { &gain, &gainLabel, gainColumn },
                                                                      { &q, &qLabel, qColumn } };
    for (auto [knob, label, columnWidth] : columns)
    {
        const int side = knob->getIdealSize();
        const auto centre = juce::Point<float> (static_cast<float> (x) + static_cast<float> (columnWidth) / 2.0f, centreY);
        knob->setBounds (juce::Rectangle<float> (static_cast<float> (side), static_cast<float> (side)).withCentre (centre).toNearestInt());
        label->setBounds (x, labelTop, columnWidth, labelHeight);
        if (knob == &gain)
        {
            ring.setBounds (knob->getBounds());
            // The dynamics icons in a row above Gain, inside the bell.
            constexpr int icon = layout::dynamicsIcon, iconGap = layout::dynamicsIconGap;
            const int rowWidth = 3 * icon + 2 * iconGap;
            dynamicsIcons.setBounds (x + (columnWidth - rowWidth) / 2, contentTop - layout::dynamicsIconsAbove, rowWidth, icon);
            int iconX = 0;
            for (auto* iconButton : { &clearDynamics, &dynamicsBypass, &dynamicsOpen })
            {
                iconButton->setBounds (iconX, 0, icon, icon);
                iconX += icon + iconGap;
            }
            // The dynamics section after it, as wide as the panel has opened, centred down the content
            // and lifted; Q and what follows move right by as much.
            const int opened = getWidth() - width;
            const int sectionTop = contentTop + (contentHeight - layout::dynamicsSectionHeight) / 2 - layout::dynamicsSectionLift;
            section.setBounds (x + columnWidth + knobGap, sectionTop, std::max (0, opened - knobGap), layout::dynamicsSectionHeight);
            x += opened;
        }
        x += columnWidth + knobGap;
    }
}

void BandPanel::paint (juce::Graphics& g)
{
    if (slot == 0)
        return;
    const auto band = bandColour();
    const auto bounds = getLocalBounds().toFloat();

    // The slab: raised, with a wash of the Band's colour from the top centre (55 % of the width across,
    // 120 % of the height down).
    g.setColour (colour::raised);
    g.fillPath (slab);
    {
        const juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (slab);
        const juce::Point<float> top { bounds.getCentreX(), 0.0f };
        const float rx = 0.55f * bounds.getWidth(), ry = 1.2f * bounds.getHeight();
        g.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, top.x, top.y));
        juce::ColourGradient wash (band.withAlpha (washTop), top, band.withAlpha (0.0f), top.translated (rx, 0.0f), true);
        wash.addColour (0.55, band.withAlpha (washMid));
        g.setGradientFill (wash);
        g.fillRect (juce::Rectangle<float> (top.x - rx, 0.0f, 2.0f * rx, rx));
    }
    // The hairline along the top edge, 0 at both ends and 45 % at the centre.
    {
        juce::Path edge;
        const float w = bounds.getWidth(), sigma = bellSigma * w;
        for (float x = 0.0f; x <= w; x += 2.0f)
        {
            const float y = static_cast<float> (bell) - static_cast<float> (bell) * std::exp (-(x - w / 2.0f) * (x - w / 2.0f) / (2.0f * sigma * sigma)) + 0.5f;
            if (x == 0.0f)
                edge.startNewSubPath (x, y);
            else
                edge.lineTo (x, y);
        }
        juce::ColourGradient line (band.withAlpha (0.0f), 0.0f, 0.0f, band.withAlpha (0.0f), w, 0.0f, false);
        line.addColour (0.5, band.withAlpha (hairlineAlpha));
        g.setGradientFill (line);
        g.strokePath (edge, juce::PathStrokeType (1.0f));
    }

    // What fades with the Bypassed Band: the dividers and the Band's number.
    const float alpha = fade.now;
    g.setColour (colour::fill2.withMultipliedAlpha (alpha));
    for (int x : dividers)
        g.fillRect (x, contentTop, 1, contentHeight);
    g.setFont (staple::font (tokens::size::fs4, staple::Weight::semiBold));
    g.setColour (band.withMultipliedAlpha (alpha));
    g.drawText (juce::String (slot), numberArea, juce::Justification::centred, false);
}

} // namespace eq1
