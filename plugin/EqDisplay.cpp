#include "EqDisplay.h"

#include "Accessibility.h"
#include "BandClipboard.h"
#include "BandMenu.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "display/AnalyzerLayer.h"
#include "display/CurvesLayer.h"
#include "display/EdgeFadeLayer.h"
#include "display/GridLayer.h"
#include "display/HandlesLayer.h"
#include "staple/LookAndFeel.h"

#include <cmath>

namespace eq1
{

namespace
{
constexpr float handleRadius = display::DisplayGeometry::handleRadius;

// Exactly the same, field by field: any change at all counts, so the comparison is exact. (Settings'
// own == would do, but its float comparison warns where it is defined.)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
bool same (const Settings& a, const Settings& b)
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
    return true;
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
} // namespace

class EqDisplay::BandHandle final : public juce::Component
{
public:
    BandHandle (EqDisplay& d, int s) : display (d), slot (s)
    {
        setName ("Band " + juce::String (slot));
        setTitle (getName());
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (true);
    }

    void focusGained (FocusChangeType) override
    {
        if (display.selected != std::set<int> { slot })
            display.select ({ slot });
    }
    void focusLost (FocusChangeType) override { display.endHeldNudge(); }

    // Tells a screen reader when what it reads has changed: the Band moved, by any means.
    void announce()
    {
        auto text = display.spokenBand (slot);
        if (text == spoken)
            return;
        spoken = std::move (text);
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    }

    EqDisplay& display;
    const int slot;

private:
    // Read-only: the keys and the Band panel adjust the Band.
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return accessibility::handler (*this, juce::AccessibilityRole::slider, [this] { return display.spokenBand (slot); });
    }

    juce::String spoken;
};

class EqDisplay::RangeGrip final : public juce::Component
{
public:
    RangeGrip (EqDisplay& d, int s) : display (d), slot (s)
    {
        setName ("Band " + juce::String (slot) + " Dynamic Range");
        setTitle (getName());
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (true);
    }

    void focusGained (FocusChangeType) override
    {
        if (display.selected != std::set<int> { slot })
            display.select ({ slot });
    }
    void focusLost (FocusChangeType) override { display.endHeldNudge(); }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const int code = key.getKeyCode();
        const auto mods = key.getModifiers();
        if ((code != juce::KeyPress::upKey && code != juce::KeyPress::downKey)
            || (mods.getRawFlags() & ~juce::ModifierKeys::shiftModifier & juce::ModifierKeys::allKeyboardModifiers) != 0)
            return false;
        staple::LookAndFeel::keyUsed (*this);
        const double step = mods.isShiftDown() ? 0.5 : 1.0;
        display.stepDynamicRange (slot, code == juce::KeyPress::upKey ? step : -step);
        return true;
    }

    // Tells a screen reader when the Dynamic Range has changed, by any means.
    void announce()
    {
        auto text = value();
        if (text == spoken)
            return;
        spoken = std::move (text);
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    }

    EqDisplay& display;
    const int slot;

private:
    juce::String value() const
    {
        const auto& parameter = *display.processor.parameterState().getParameter (parameters::dynamicRangeId (slot));
        return accessibility::spokenValue (parameter, parameter.getValue());
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return accessibility::handler (*this, juce::AccessibilityRole::slider, [this] { return value(); });
    }

    juce::String spoken;
};

EqDisplay::EqDisplay (PluginProcessor& p, BandEditing& e) : processor (p), editing (e)
{
    setName ("EQ Display");
    setTitle ("EQ display");
    setWantsKeyboardFocus (true);
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        auto& handle = handles[static_cast<size_t> (slot - 1)];
        handle = std::make_unique<BandHandle> (*this, slot);
        addChildComponent (*handle);
        auto& grip = rangeGrips[static_cast<size_t> (slot - 1)];
        grip = std::make_unique<RangeGrip> (*this, slot);
        addChildComponent (*grip);
    }
    shown = heardSettings();
    tapSamples.resize (1 << 16);
    // An editor opened under Global Bypass shows it at once.
    globalBypassFade = isGlobalBypassOn() ? 1.0f : 0.0f;
    lastFadeStep = juce::Time::getMillisecondCounter();
    startTimerHz (60);
}

bool EqDisplay::updateAnalyzer()
{
    analyzer = processor.analyzerSettings();
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const auto now = juce::Time::getMillisecondCounter();
    const double seconds = lastFrame == 0 ? 1.0 / 60.0 : juce::jlimit (0.001, 0.25, (now - lastFrame) / 1000.0);
    lastFrame = now;

    // Peak Hold starts afresh on a spectrum it wasn't held for last frame: switched to, or turned on.
    // The spectrum forgets it too when prepared again for a new Resolution or sample rate.
    auto* toHold = analyzer.peakHold ? spectrumToGrab() : nullptr;
    if (toHold != nullptr && toHold != held)
        toHold->clearPeakHold();
    held = toHold;

    const std::pair<AnalysisTap, AnalyzerSpectrum*> taps[] = { { AnalysisTap::PreEq, &preEq },
                                                               { AnalysisTap::PostEq, &postEq },
                                                               { AnalysisTap::Sidechain, &sidechain } };
    const bool showing[] = { analyzer.showPreEq, analyzer.showPostEq, analyzer.showSidechain };
    for (size_t i = 0; i < std::size (taps); ++i)
    {
        auto [tap, spectrum] = taps[i];
        if (! spectrum->isPrepared() || ! juce::exactlyEqual (spectrum->getSampleRate(), sampleRate)
            || spectrum->getResolution() != analyzer.resolution)
            spectrum->prepare (sampleRate, analyzer.resolution);
        // Always drained, so the taps hold only the newest samples when a spectrum is shown again;
        // analysed only when shown.
        const int count = processor.readAnalysis (tap, tapSamples.data(), static_cast<int> (tapSamples.size()));
        spectrum->push (tapSamples.data(), count);
        if (showing[i])
            spectrum->update (seconds, analyzer.speed);
    }
    return analyzer.showPreEq || analyzer.showPostEq || analyzer.showSidechain;
}

float EqDisplay::spectrumYAt (const AnalyzerSpectrum& spectrum, float x) const
{
    return display::spectrumYAt (geometry(), analyzer, spectrum, x);
}

AnalyzerSpectrum* EqDisplay::spectrumToGrab()
{
    return analyzer.showPostEq ? &postEq : analyzer.showPreEq ? &preEq : nullptr;
}

Settings EqDisplay::heardSettings() const
{
    auto settings = editing.settings();
    for (auto& band : settings.bands)
        band = scaledByGainScale (band, settings.gainScale);
    return settings;
}

EqDisplay::~EqDisplay()
{
    endHeldNudge();
    releaseSolo();
}

void EqDisplay::resized()
{
    placeHandles();
}

void EqDisplay::placeHandles()
{
    const bool reorder = focusedSlot() == 0;
    std::vector<int> inUse;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
        auto& handle = *handles[static_cast<size_t> (slot - 1)];
        handle.setVisible (band.inUse);
        if (! band.inUse)
            continue;
        inUse.push_back (slot);
        handle.announce();
        handle.setBounds (juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (handleOf (band)).getSmallestIntegerContainer());
    }
    std::array<bool, numBandSlots> gripShown {};
    for (const auto& grip : display::dynamicRangeGrips (geometry(), frame()))
    {
        gripShown[static_cast<size_t> (grip.slot - 1)] = true;
        auto& element = *rangeGrips[static_cast<size_t> (grip.slot - 1)];
        element.setBounds (display::gripArea (grip.centre).getSmallestIntegerContainer());
        element.announce();
    }
    for (int slot = 1; slot <= numBandSlots; ++slot)
        rangeGrips[static_cast<size_t> (slot - 1)]->setVisible (gripShown[static_cast<size_t> (slot - 1)]);
    if (! reorder)
        return;
    // By Frequency, the lower Band Slot first on a tie (std::stable_sort keeps slot order), each grip
    // after its Band.
    std::stable_sort (inUse.begin(), inUse.end(), [this] (int a, int b) {
        return shown.bands[static_cast<size_t> (a - 1)].frequency < shown.bands[static_cast<size_t> (b - 1)].frequency;
    });
    for (size_t i = 0; i < inUse.size(); ++i)
    {
        handles[static_cast<size_t> (inUse[i] - 1)]->setExplicitFocusOrder (2 * static_cast<int> (i) + 1);
        rangeGrips[static_cast<size_t> (inUse[i] - 1)]->setExplicitFocusOrder (2 * static_cast<int> (i) + 2);
    }
}

int EqDisplay::gripAt (juce::Point<float> position) const
{
    const auto grips = display::dynamicRangeGrips (geometry(), frame());
    for (auto grip = grips.rbegin(); grip != grips.rend(); ++grip)
        if (display::gripArea (grip->centre).contains (position))
            return grip->slot;
    return 0;
}

void EqDisplay::stepDynamicRange (int slot, double db)
{
    if (! nudging)
    {
        processor.editHistory().beginTransaction();
        nudging = true;
    }
    editing.setDynamicRange (slot, editing.band (slot).dynamicRange + db);
    shown = heardSettings();
    placeHandles();
    repaint();
}

juce::String EqDisplay::spokenBand (int slot) const
{
    auto& state = processor.parameterState();
    const auto band = editing.band (slot);
    const auto value = [&state] (const juce::String& id) {
        const auto& parameter = *state.getParameter (id);
        return accessibility::spokenValue (parameter, parameter.getValue());
    };
    juce::StringArray parts { parameters::shapeNames()[static_cast<int> (band.shape)], value (parameters::frequencyId (slot)) };
    if (hasGain (band.shape))
        parts.add (value (parameters::gainId (slot)));
    parts.add ("Q " + value (parameters::qId (slot)));
    if (band.bypass)
        parts.add ("Bypassed");
    if (isDynamic (band))
        parts.add ("Dynamic Band");
    return parts.joinIntoString (", ");
}

std::unique_ptr<juce::AccessibilityHandler> EqDisplay::createAccessibilityHandler()
{
    return accessibility::handler (*this, juce::AccessibilityRole::group, nullptr);
}

int EqDisplay::focusedSlot() const
{
    for (const auto& handle : handles)
        if (handle->hasKeyboardFocus (false))
            return handle->slot;
    for (const auto& grip : rangeGrips)
        if (grip->hasKeyboardFocus (false))
            return grip->slot;
    return 0;
}

void EqDisplay::releaseSolo()
{
    if (soloedSlot == 0)
        return;
    soloedSlot = 0;
    processor.setSolo (0);
    repaint();
}

void EqDisplay::timerCallback()
{
    if (heldSlot != 0 && juce::Time::getMillisecondCounter() - heldSince >= soloHoldMilliseconds)
    {
        soloedSlot = heldSlot;
        heldSlot = 0;
        processor.setSolo (soloedSlot);
        repaint();
    }
    // A Band deleted, or taken out of use by automation, while Soloed lets go of its Solo for good.
    if (soloedSlot != 0 && ! editing.band (soloedSlot).inUse)
        releaseSolo();
    // A heard Gain changed beyond the Display Range, from anywhere, zooms it out.
    processor.fitDisplayRangeToHeardGains();
    const bool fading = stepFades();

    if (updateAnalyzer())
    {
        // The spectra move every frame.
        shown = heardSettings();
        shownRangeDb = processor.displayRangeDb();
        placeHandles();
        repaint();
        return;
    }

    const bool messageExpired = allInUseMessageUntil != 0 && juce::Time::getMillisecondCounter() > allInUseMessageUntil;
    if (messageExpired)
        allInUseMessageUntil = 0;
    // Host Automation, the Band panel and a restored state change the parameters and the range too.
    const auto latest = heardSettings();
    const int range = processor.displayRangeDb();
    // Dynamic Bands move by themselves.
    bool liveGainsMoved = false;
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const double live = drawnGain (slot, latest.bands[static_cast<size_t> (slot - 1)]);
        liveGainsMoved = liveGainsMoved || ! juce::exactlyEqual (live, shownLiveGains[static_cast<size_t> (slot - 1)]);
        shownLiveGains[static_cast<size_t> (slot - 1)] = live;
    }
    if (fading)
        repaint();
    if (same (latest, shown) && range == shownRangeDb && ! messageExpired && ! liveGainsMoved)
        return;
    shown = latest;
    shownRangeDb = range;

    // A Band deleted from elsewhere is no longer selected.
    auto stillInUse = selected;
    std::erase_if (stillInUse, [this] (int slot) { return ! shown.bands[static_cast<size_t> (slot - 1)].inUse; });
    if (stillInUse != selected)
        select (stillInUse);
    placeHandles();
    repaint();
}

bool EqDisplay::isGlobalBypassOn() const
{
    return processor.parameterState().getRawParameterValue (parameters::globalBypassId)->load() >= 0.5f;
}

bool EqDisplay::stepFades()
{
    const auto now = juce::Time::getMillisecondCounter();
    const auto elapsed = static_cast<float> (now - lastFadeStep);
    lastFadeStep = now;
    const auto toward = [elapsed] (float& fade, bool on, int milliseconds) {
        const float target = on ? 1.0f : 0.0f;
        const float moved = juce::jlimit (fade - elapsed / static_cast<float> (milliseconds), fade + elapsed / static_cast<float> (milliseconds), target);
        const bool changed = ! juce::exactlyEqual (moved, fade);
        fade = moved;
        return changed;
    };
    namespace motion = staple::tokens::motion;
    bool moved = toward (globalBypassFade, isGlobalBypassOn(), motion::globalBypassFadeMs);
    for (int slot = 1; slot <= numBandSlots; ++slot)
        moved = toward (hoverFades[static_cast<size_t> (slot - 1)], slot == hoveredSlot, motion::hoverFadeMs) || moved;
    // The ghost fades in as it appears, and goes at once.
    if (ghost())
        moved = toward (ghostFade, true, staple::tokens::ghost::fadeInMs) || moved;
    else if (std::exchange (ghostFade, 0.0f) > 0.0f)
        moved = true;
    return moved;
}

std::optional<display::Ghost> EqDisplay::ghost() const
{
    // A menu is modal while it shows.
    if (dragging || marquee || rangeDragSlot != 0 || pressedOnEmpty || grabFrequency || juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0)
        return std::nullopt;
    int inUse = 0;
    for (const auto& band : shown.bands)
        inUse += band.inUse ? 1 : 0;
    if (inUse == numBandSlots)
        return std::nullopt;
    if (pointer)
    {
        if (slotAt (*pointer) != 0 || gripAt (*pointer) != 0 || bandAreaAt (*pointer) != 0)
            return std::nullopt;
        return display::ghostBell (geometry(), *pointer);
    }
    if (inUse == 0)
        return display::restingGhost (geometry());
    return std::nullopt;
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    pointer = e.position;
    if (ghost() || ghostFade > 0.0f)
        repaint();
    // A handle, or else a Band's filled curve.
    hoveredSlot = slotAt (e.position);
    if (hoveredSlot == 0)
        hoveredSlot = bandAreaAt (e.position);
}

void EqDisplay::mouseExit (const juce::MouseEvent&)
{
    hoveredSlot = 0;
    pointer.reset();
    repaint();
}

display::DisplayGeometry EqDisplay::geometry() const
{
    return { .width = getWidth(), .height = getHeight(), .rangeDb = processor.displayRangeDb() };
}

float EqDisplay::xOf (double frequency) const { return geometry().xOf (frequency); }
double EqDisplay::frequencyAt (float x) const { return geometry().frequencyAt (x); }
float EqDisplay::yOf (double db) const { return geometry().yOf (db); }
double EqDisplay::dbAt (float y) const { return geometry().dbAt (y); }
juce::Point<float> EqDisplay::handleOf (const BandSettings& band) const { return geometry().handleOf (band); }

double EqDisplay::drawnGain (int slot, const BandSettings& band) const
{
    return isDynamic (band) && band.inUse && ! band.dynamicsBypass ? processor.liveGainDb (slot) : band.gain;
}

int EqDisplay::slotAt (juce::Point<float> position) const
{
    // A selected handle wins where handles overlap, as it is drawn on top, then the highest slot. Each
    // reaches as far as it is drawn, and at least 9 px.
    namespace handle = staple::tokens::handle;
    for (const bool onTop : { true, false })
        for (int slot = numBandSlots; slot >= 1; --slot)
        {
            const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
            const bool isSelected = selected.contains (slot);
            const float reach = std::max (handle::hitRadius, (isSelected ? handle::selectedDiameter : handle::diameter) / 2.0f);
            if (band.inUse && isSelected == onTop && handleOf (band).getDistanceFrom (position) <= reach)
                return slot;
        }
    return 0;
}

void EqDisplay::select (std::set<int> slots)
{
    selected = std::move (slots);
    if (onSelectionChanged)
        onSelectionChanged (selected.empty() ? 0 : *selected.rbegin());
    // The selected Band shows its grip.
    placeHandles();
    repaint();
}

display::DisplayFrame EqDisplay::frame() const
{
    display::DisplayFrame result { .bands = shown,
                                   .selected = selected,
                                   .soloedSlot = soloedSlot,
                                   .dragging = dragging,
                                   .marquee = marquee,
                                   .allInUseMessage = allInUseMessageUntil != 0,
                                   // Before the host has prepared the plugin, the curves are drawn as at 48 kHz.
                                   .sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0,
                                   .mono = ! processor.isStereoPlacementAvailable(),
                                   .hover = hoverFades,
                                   .globalBypass = globalBypassFade };
    for (int slot = 1; slot <= numBandSlots; ++slot)
        result.drawnGains[static_cast<size_t> (slot - 1)] = drawnGain (slot, shown.bands[static_cast<size_t> (slot - 1)]);
    return result;
}

int EqDisplay::bandAreaAt (juce::Point<float> position) const
{
    return display::bandAreaAt (geometry(), frame(), position);
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto shape = geometry();
    const auto frame = this->frame();
    g.fillAll (staple::tokens::colour::bg0);
    display::paintGrid (g, shape);
    display::paintAnalyzer (g, shape, { .settings = analyzer, .preEq = preEq, .postEq = postEq, .sidechain = sidechain, .held = held });
    display::paintCurves (g, shape, frame);
    // The handles and labels go over the edge fades, unfaded.
    display::paintEdgeFades (g, shape);
    const auto shownGhost = ghostFade > 0.0f ? ghost() : std::nullopt;
    display::paintLabels (g, shownGhost ? display::fadedForGhost (display::gridLabels (shape), shape, *shownGhost) : display::gridLabels (shape));
    display::paintLabels (g, display::analyzerScaleLabels (shape, analyzer));
    if (shownGhost)
        display::paintGhost (g, shape, *shownGhost, frame.sampleRate, ghostFade);
    display::paintHandles (g, shape, frame);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    // Right-click, or Ctrl-click on macOS: never a Solo, a drag or a marquee.
    if (e.mods.isPopupMenu())
    {
        if (! dragging && ! marquee)
            showMenu (e);
        return;
    }
    if (! e.mods.isLeftButtonDown())
        return;
    grabKeyboardFocus();
    dragStart = e.position;
    const bool adding = e.mods.isShiftDown() || e.mods.isCommandDown();
    const int slot = slotAt (e.position);
    if (const int gripped = slot == 0 ? gripAt (e.position) : 0; gripped != 0)
    {
        // A grip: never a Band drag, a marquee or a Solo.
        if (selected != std::set<int> { gripped })
            select ({ gripped });
        const auto& band = shown.bands[static_cast<size_t> (gripped - 1)];
        rangeDragSlot = gripped;
        rangeDragStartEnd = band.gain + band.dynamicRange;
        editing.beginDynamicRangeDrag (gripped);
        return;
    }
    if (slot == 0 && ! adding)
    {
        // Spectrum Grab: pressing on the spectrum, near its drawn line, grabs the peak there once the
        // mouse moves.
        if (const auto* spectrum = spectrumToGrab(); spectrum != nullptr && std::abs (spectrumYAt (*spectrum, e.position.x) - e.position.y) <= 12.0f)
        {
            grabFrequency = spectrum->peakNear (frequencyAt (e.position.x));
            return;
        }
    }
    if (slot == 0)
    {
        pressedOnEmpty = true;
        pressAdding = adding;
        return;
    }

    if (adding)
    {
        // Shift or Cmd toggles the Band; one added can be dragged with the rest at once.
        auto toggled = selected;
        if (! toggled.erase (slot))
            toggled.insert (slot);
        select (toggled);
        if (! selected.contains (slot))
            return;
    }
    else if (! selected.contains (slot))
    {
        select ({ slot });
    }
    if (! adding)
    {
        heldSlot = slot;
        heldSince = juce::Time::getMillisecondCounter();
    }
    editing.beginDrag (std::vector<int> (selected.begin(), selected.end()));
    dragging = true;
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (rangeDragSlot != 0)
    {
        // The range's end follows the mouse from where it was.
        editing.dragDynamicRangeTo (rangeDragStartEnd + dbAt (e.position.y) - dbAt (dragStart.y));
        shown = heardSettings();
        placeHandles();
        repaint();
        return;
    }
    // Moving before the hold Solos makes it a drag; once Soloed, the Band can be dragged while heard.
    if (heldSlot != 0 && e.getDistanceFromDragStart() > dragThreshold)
        heldSlot = 0;
    if (grabFrequency && e.getDistanceFromDragStart() > dragThreshold)
    {
        if (const auto grabbed = editing.grab (*grabFrequency))
        {
            select ({ *grabbed });
            dragging = grabbing = true;
        }
        else
        {
            allInUseMessageUntil = juce::Time::getMillisecondCounter() + 2500;
        }
        grabFrequency.reset();
    }
    if (pressedOnEmpty && ! marquee && e.getDistanceFromDragStart() > dragThreshold)
    {
        selectedBeforeMarquee = pressAdding ? selected : std::set<int> {};
        marquee = juce::Rectangle<float> (dragStart, dragStart);
    }
    if (marquee)
    {
        marquee = juce::Rectangle<float> (dragStart, e.position);
        auto inside = selectedBeforeMarquee;
        for (int slot = 1; slot <= numBandSlots; ++slot)
        {
            const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
            if (band.inUse && marquee->contains (handleOf (band)))
                inside.insert (slot);
        }
        select (inside);
        return;
    }
    if (! dragging)
        return;
    // A grabbed Band stays on its peak: the drag sets its Gain.
    const double frequencyRatio = grabbing ? 1.0 : frequencyAt (e.position.x) / frequencyAt (dragStart.x);
    editing.dragBy (frequencyRatio, dbAt (e.position.y) - dbAt (dragStart.y));
    shown = heardSettings();
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (std::exchange (rangeDragSlot, 0) != 0)
        editing.endDynamicRangeDrag();
    if (std::exchange (pressedOnEmpty, false) && ! marquee)
    {
        // A click: inside a Band's filled curve selects it (Shift or Cmd toggles it), on empty space
        // clears the selection.
        const int slot = bandAreaAt (dragStart);
        auto toggled = pressAdding ? selected : std::set<int> {};
        if (slot != 0 && ! toggled.erase (slot))
            toggled.insert (slot);
        select (toggled);
    }
    if (dragging)
    {
        // The drag's Gain offset follows the mouse under the range it began with, so the range zooms
        // only now.
        editing.endDrag();
        processor.fitDisplayRangeToHeardGains();
    }
    dragging = grabbing = false;
    heldSlot = 0;
    releaseSolo();
    grabFrequency.reset();
    marquee.reset();
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || slotAt (e.position) != 0)
        return;
    if (const int gripped = gripAt (e.position); gripped != 0)
    {
        editing.setDynamicRange (gripped, 0.0);
        shown = heardSettings();
        placeHandles();
        repaint();
        return;
    }
    if (const auto slot = editing.add (frequencyAt (e.position.x), dbAt (e.position.y)))
    {
        shown = heardSettings();
        select ({ *slot });
        return;
    }
    allInUseMessageUntil = juce::Time::getMillisecondCounter() + 2500;
    repaint();
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // The Band under the mouse, or else the selected ones.
    std::set<int> targets;
    if (const int slot = slotAt (e.position); slot != 0)
        targets = { slot };
    else
        targets = selected;
    const double factor = std::pow (2.0, static_cast<double> (wheel.deltaY) * (wheel.isReversed ? -1.0 : 1.0));
    editing.scaleQ ({ targets.begin(), targets.end() }, factor);
    shown = heardSettings();
    repaint();
}

void EqDisplay::showMenu (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int slot = slotAt (e.position);
    if (slot != 0 && ! selected.contains (slot))
        select ({ slot });
    const juce::Component::SafePointer<EqDisplay> display (this);
    const BandMenu menu { editing,
                          slot == 0 ? std::vector<int> {} : std::vector<int> (selected.begin(), selected.end()),
                          processor.isStereoPlacementAvailable(),
                          [display] {
                              if (display != nullptr)
                                  display->deleteSelection();
                          },
                          [display] {
                              if (display != nullptr)
                                  display->selectAll();
                          },
                          [display] (std::vector<int> slots) {
                              if (display != nullptr)
                                  display->select ({ slots.begin(), slots.end() });
                          },
                          juce::SystemClipboard::getTextFromClipboard(),
                          [] (const juce::String& text) { juce::SystemClipboard::copyTextToClipboard (text); } };
    // Closed unchosen if the display goes, so the Band actions never outlive the editing they use. A
    // menu is a window of its own: it draws with the editor's look only when given it.
    auto popup = menu.build();
    popup.setLookAndFeel (&getLookAndFeel());
    popup.showMenuAsync (juce::PopupMenu::Options().withDeletionCheck (*this).withTargetComponent (this).withMousePosition());
}

void EqDisplay::selectAll()
{
    std::set<int> inUse;
    for (int slot = 1; slot <= numBandSlots; ++slot)
        if (editing.band (slot).inUse)
            inUse.insert (slot);
    select (inUse);
}

void EqDisplay::copySelection()
{
    std::vector<BandSettings> bands;
    for (int slot : selected)
        bands.push_back (editing.band (slot));
    juce::SystemClipboard::copyTextToClipboard (captureBands (bands).toXmlString());
}

bool EqDisplay::paste()
{
    const auto pasted = editing.paste (clipboardBands (juce::SystemClipboard::getTextFromClipboard()));
    if (pasted.empty())
        return false;
    select ({ pasted.begin(), pasted.end() });
    shown = heardSettings();
    return true;
}

void EqDisplay::deleteSelection()
{
    editing.deleteBands ({ selected.begin(), selected.end() });
    select ({});
    shown = heardSettings();
    placeHandles();
}

void EqDisplay::nudgeSelection (double semitones, double heardDb)
{
    if (! nudging)
    {
        processor.editHistory().beginTransaction();
        nudging = true;
    }
    editing.nudge ({ selected.begin(), selected.end() }, semitones, heardDb);
    shown = heardSettings();
    placeHandles();
    repaint();
}

void EqDisplay::endHeldNudge()
{
    if (! std::exchange (nudging, false))
        return;
    processor.editHistory().endTransaction();
    // As after a drag, a Gain moved beyond the Display Range zooms it out.
    processor.fitDisplayRangeToHeardGains();
}

bool EqDisplay::keyStateChanged (bool isKeyDown)
{
    if (! isKeyDown)
        endHeldNudge();
    return false;
}

void EqDisplay::focusLost (FocusChangeType)
{
    endHeldNudge();
}

bool EqDisplay::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && ! selected.empty())
    {
        // A Band with focus hands it to the next Band in Tab's order that is left, else the one before,
        // else the display.
        const int focused = focusedSlot();
        BandHandle* next = nullptr;
        if (focused != 0)
        {
            std::vector<BandHandle*> order;
            for (auto& handle : handles)
                if (handle->isVisible() && (handle->slot == focused || ! selected.contains (handle->slot)))
                    order.push_back (handle.get());
            std::stable_sort (order.begin(), order.end(), [] (auto* a, auto* b) { return a->getExplicitFocusOrder() < b->getExplicitFocusOrder(); });
            const auto at = std::find_if (order.begin(), order.end(), [focused] (auto* h) { return h->slot == focused; });
            if (std::next (at) != order.end())
                next = *std::next (at);
            else if (at != order.begin())
                next = *std::prev (at);
        }
        deleteSelection();
        if (next != nullptr)
            next->grabKeyboardFocus();
        else if (focused != 0)
            grabKeyboardFocus();
        return true;
    }
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    const bool horizontal = code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey;
    const bool vertical = code == juce::KeyPress::upKey || code == juce::KeyPress::downKey;
    if ((horizontal || vertical) && ! selected.empty()
        && (mods.getRawFlags() & ~juce::ModifierKeys::shiftModifier & juce::ModifierKeys::allKeyboardModifiers) == 0)
    {
        staple::LookAndFeel::keyUsed (*this);
        const double sign = code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey ? 1.0 : -1.0;
        const bool fine = mods.isShiftDown();
        if (horizontal)
            nudgeSelection (sign * (fine ? 0.1 : 1.0), 0.0);
        else
            nudgeSelection (0.0, sign * (fine ? 0.05 : 0.5));
        return true;
    }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAll();
        return true;
    }
    const bool cut = key == juce::KeyPress ('x', juce::ModifierKeys::commandModifier, 0);
    if ((cut || key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)) && ! selected.empty())
    {
        copySelection();
        if (cut)
            deleteSelection();
        return true;
    }
    if (key == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0))
        return paste();
    return false;
}

} // namespace eq1
