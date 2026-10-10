#include "EqDisplay.h"

#include "BandMenu.h"
#include "PluginProcessor.h"
#include "eq1/Response.h"

#include <cmath>

namespace eq1
{

namespace
{
constexpr double lowestFrequency = 10.0, highestFrequency = 30000.0;
constexpr float handleRadius = 9.0f;
constexpr float pixelStep = 2.0f; // the curves are evaluated every this many pixels
constexpr float ringRadius = handleRadius + 5.0f;

// Hues a golden ratio apart, so Bands in neighbouring slots look different.
juce::Colour colourOf (int slot)
{
    return juce::Colour::fromHSV (std::fmod (0.03f + 0.618034f * static_cast<float> (slot - 1), 1.0f), 0.65f, 0.95f, 1.0f);
}

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

juce::String frequencyText (double frequency)
{
    return frequency >= 1000.0 ? juce::String (frequency / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (frequency)) + " Hz";
}
} // namespace

EqDisplay::EqDisplay (PluginProcessor& p, BandEditing& e) : processor (p), editing (e)
{
    setWantsKeyboardFocus (true);
    shown = heardSettings();
    tapSamples.resize (1 << 16);
    startTimerHz (60);
}

bool EqDisplay::updateAnalyzer()
{
    analyzer = processor.analyzerSettings();
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const auto now = juce::Time::getMillisecondCounter();
    const double seconds = lastFrame == 0 ? 1.0 / 60.0 : juce::jlimit (0.001, 0.25, (now - lastFrame) / 1000.0);
    lastFrame = now;

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
    const double level = spectrum.levelDb (frequencyAt (x), analyzer.tiltDbPerOctave);
    return static_cast<float> (juce::jlimit (0.0, 1.0, -level / analyzer.rangeDb) * getHeight());
}

const AnalyzerSpectrum* EqDisplay::spectrumToGrab() const
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
    releaseSolo();
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

    if (updateAnalyzer())
    {
        // The spectra move every frame.
        shown = heardSettings();
        shownRangeDb = processor.displayRangeDb();
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
    if (same (latest, shown) && range == shownRangeDb && ! messageExpired && ! liveGainsMoved)
        return;
    shown = latest;
    shownRangeDb = range;

    // A Band deleted from elsewhere is no longer selected.
    auto stillInUse = selected;
    std::erase_if (stillInUse, [this] (int slot) { return ! shown.bands[static_cast<size_t> (slot - 1)].inUse; });
    if (stillInUse != selected)
        select (stillInUse);
    repaint();
}

float EqDisplay::xOf (double frequency) const
{
    return static_cast<float> (std::log (frequency / lowestFrequency) / std::log (highestFrequency / lowestFrequency) * getWidth());
}

double EqDisplay::frequencyAt (float x) const
{
    return lowestFrequency * std::pow (highestFrequency / lowestFrequency, juce::jlimit (0.0, 1.0, static_cast<double> (x) / getWidth()));
}

float EqDisplay::yOf (double db) const
{
    const auto range = static_cast<float> (processor.displayRangeDb());
    const float half = static_cast<float> (getHeight()) * 0.5f;
    return half - static_cast<float> (db) / range * (half - handleRadius);
}

double EqDisplay::dbAt (float y) const
{
    const auto range = static_cast<double> (processor.displayRangeDb());
    const double half = getHeight() * 0.5;
    return (half - y) / (half - handleRadius) * range;
}

juce::Point<float> EqDisplay::handleOf (const BandSettings& band) const
{
    // A Shape without Gain sits on the 0 dB line; one beyond the display range sits at its edge.
    const auto range = static_cast<double> (processor.displayRangeDb());
    const double gain = hasGain (band.shape) ? juce::jlimit (-range, range, band.gain) : 0.0;
    return { xOf (band.frequency), yOf (gain) };
}

double EqDisplay::drawnGain (int slot, const BandSettings& band) const
{
    return isDynamic (band) && band.inUse && ! band.dynamicsBypass ? processor.liveGainDb (slot) : band.gain;
}

int EqDisplay::slotAt (juce::Point<float> position) const
{
    // The highest slot wins where handles overlap, as it is drawn on top.
    for (int slot = numBandSlots; slot >= 1; --slot)
    {
        const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
        if (band.inUse && handleOf (band).getDistanceFrom (position) <= handleRadius)
            return slot;
    }
    return 0;
}

void EqDisplay::select (std::set<int> slots)
{
    selected = std::move (slots);
    if (onSelectionChanged)
        onSelectionChanged (selected.empty() ? 0 : *selected.rbegin());
    repaint();
}

juce::Path EqDisplay::curve (const std::vector<double>& db) const
{
    juce::Path path;
    const auto range = static_cast<double> (processor.displayRangeDb());
    for (size_t i = 0; i < db.size(); ++i)
    {
        const juce::Point<float> point { static_cast<float> (i) * pixelStep, yOf (juce::jlimit (-range * 1.5, range * 1.5, db[i])) };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    return path;
}

void EqDisplay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    // Grid: decades and their halves, and Gain lines a quarter of the range apart.
    g.setFont (11.0f);
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xOf (f);
        g.setColour (juce::Colour (0x20ffffff));
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, static_cast<float> (getHeight()));
        g.setColour (juce::Colour (0x60ffffff));
        g.drawText (f >= 1000.0 ? juce::String (juce::roundToInt (f / 1000.0)) + "k" : juce::String (juce::roundToInt (f)),
                    juce::Rectangle<float> (x + 3.0f, static_cast<float> (getHeight()) - 16.0f, 40.0f, 14.0f), juce::Justification::left);
    }
    // The Analyzer behind everything: pre-EQ filled, post-EQ filled and outlined, the Sidechain outlined.
    const auto spectrumLine = [&] (const AnalyzerSpectrum& spectrum) {
        juce::Path line;
        for (float x = 0.0f; x <= static_cast<float> (getWidth()); x += pixelStep)
        {
            const juce::Point<float> point { x, spectrumYAt (spectrum, x) };
            if (x == 0.0f)
                line.startNewSubPath (point);
            else
                line.lineTo (point);
        }
        return line;
    };
    const auto areaUnder = [&] (juce::Path line) {
        line.lineTo (line.getCurrentPosition().withY (static_cast<float> (getHeight())));
        line.lineTo (0.0f, static_cast<float> (getHeight()));
        line.closeSubPath();
        return line;
    };
    if (analyzer.showPreEq)
    {
        g.setColour (juce::Colour (0x302f8fd0));
        g.fillPath (areaUnder (spectrumLine (preEq)));
    }
    if (analyzer.showPostEq)
    {
        const auto line = spectrumLine (postEq);
        g.setColour (juce::Colour (0x18ffffff));
        g.fillPath (areaUnder (line));
        g.setColour (juce::Colour (0x70a0d8ff));
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }
    if (analyzer.showSidechain)
    {
        g.setColour (juce::Colour (0xa0e0a040));
        g.strokePath (spectrumLine (sidechain), juce::PathStrokeType (1.0f));
    }

    const int range = processor.displayRangeDb();
    for (int step = -2; step <= 2; ++step)
    {
        const double db = range * step / 2.0;
        const float y = yOf (db);
        g.setColour (step == 0 ? juce::Colour (0x40ffffff) : juce::Colour (0x18ffffff));
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, static_cast<float> (getWidth()));
        g.setColour (juce::Colour (0x60ffffff));
        // Above its line, except at the top edge.
        const float labelY = y - 14.0f < 0.0f ? y + 2.0f : y - 14.0f;
        g.drawText ((db > 0 ? "+" : "") + juce::String (db, 0), juce::Rectangle<float> (4.0f, labelY, 40.0f, 12.0f),
                    juce::Justification::left);
    }

    // Each Band's own curve, then the whole EQ's: the sum of the Bands that are playing. Nothing plays
    // above Nyquist, so the curves stay level from there. Before the host has prepared the plugin,
    // they are drawn as at 48 kHz.
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    std::vector<double> frequencies;
    for (float x = 0.0f; x <= static_cast<float> (getWidth()); x += pixelStep)
        frequencies.push_back (std::min (frequencyAt (x), 0.4999 * sampleRate));
    // On mono a Side Band has nothing to process (#6): it plays no part in the whole EQ's curve.
    const bool mono = ! processor.isStereoPlacementAvailable();
    std::vector<double> total (frequencies.size(), 0.0), bandDb (frequencies.size());
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const bool silent = band.bypass || (mono && band.placement == StereoPlacement::Side);
        auto live = band;
        live.gain = drawnGain (slot, band);
        bandResponseDb (live, frequencies.data(), bandDb.data(), static_cast<int> (bandDb.size()), sampleRate);
        if (! silent)
            for (size_t i = 0; i < total.size(); ++i)
                total[i] += bandDb[i];
        g.setColour (colourOf (slot).withAlpha (silent ? 0.12f : selected.contains (slot) ? 0.6f : 0.3f));
        g.strokePath (curve (bandDb), juce::PathStrokeType (1.2f));
    }
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (curve (total), juce::PathStrokeType (2.0f));

    // Handles, numbered by Band Slot.
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
        if (! band.inUse)
            continue;
        const auto centre = handleOf (band);
        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);
        g.setColour (colourOf (slot).withAlpha (band.bypass ? 0.35f : 1.0f));
        g.fillEllipse (circle);
        if (selected.contains (slot))
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (circle.expanded (2.0f), 1.5f);
        }
        g.setColour (juce::Colours::black);
        g.drawText (juce::String (slot), circle, juce::Justification::centred);
        if (isDynamic (band))
        {
            // The Dynamic Range ring: from the top, clockwise for a boost and anticlockwise for a cut, half
            // a turn for 30 dB, shortened where Live Gain would go beyond +/-30 dB. Live Gain's movement
            // is drawn on top of it.
            const auto angleOf = [] (double db) { return static_cast<float> (db / liveGainLimitDb * juce::MathConstants<double>::pi); };
            const double reach = juce::jlimit (-liveGainLimitDb, liveGainLimitDb, band.gain + band.dynamicRange) - band.gain;
            const auto arc = [&] (double db) {
                juce::Path path;
                path.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, 0.0f, angleOf (db), true);
                return path;
            };
            g.setColour (juce::Colour (0xffd04040).withAlpha (band.dynamicsBypass ? 0.35f : 0.9f));
            g.strokePath (arc (reach), juce::PathStrokeType (3.0f));
            if (! band.dynamicsBypass)
            {
                g.setColour (juce::Colours::yellow);
                g.strokePath (arc (drawnGain (slot, band) - band.gain), juce::PathStrokeType (3.0f));
            }
        }
        if (slot == soloedSlot)
        {
            g.setColour (juce::Colours::yellow);
            g.drawEllipse (circle.expanded (5.0f), 2.0f);
            g.drawText ("Solo", circle.withY (circle.getY() - 22.0f).expanded (20.0f, 0.0f), juce::Justification::centred);
        }
    }

    // Values beside the Bands being dragged.
    if (dragging)
        for (int slot : selected)
        {
            const auto& band = shown.bands[static_cast<size_t> (slot - 1)];
            juce::String text = frequencyText (band.frequency);
            if (hasGain (band.shape))
                text << "  " << (band.gain > 0.0 ? "+" : "") << juce::String (band.gain, 1) << " dB";
            text << "  Q " << juce::String (band.q, 2);
            const auto centre = handleOf (band);
            auto box = juce::Rectangle<float> (170.0f, 18.0f).withPosition (centre.x + 12.0f, centre.y - 26.0f);
            box = box.constrainedWithin (getLocalBounds().toFloat());
            g.setColour (juce::Colour (0xd0000000));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (juce::Colours::white);
            g.drawText (text, box, juce::Justification::centred);
        }

    if (marquee)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (*marquee);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawRect (*marquee, 1.0f);
    }

    if (allInUseMessageUntil != 0)
    {
        const auto box = getLocalBounds().toFloat().withSizeKeepingCentre (300.0f, 30.0f).withY (12.0f);
        g.setColour (juce::Colour (0xe0402020));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("All 24 Bands are in use", box, juce::Justification::centred);
    }
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
        selectedBeforeMarquee = adding ? selected : std::set<int> {};
        select (selectedBeforeMarquee);
        marquee = juce::Rectangle<float> (e.position, e.position);
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
    if (dragging)
        editing.endDrag();
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
                          } };
    // Closed unchosen if the display goes, so the Band actions never outlive the editing they use.
    menu.build().showMenuAsync (juce::PopupMenu::Options().withDeletionCheck (*this).withMousePosition());
}

void EqDisplay::selectAll()
{
    std::set<int> inUse;
    for (int slot = 1; slot <= numBandSlots; ++slot)
        if (editing.band (slot).inUse)
            inUse.insert (slot);
    select (inUse);
}

void EqDisplay::deleteSelection()
{
    editing.deleteBands ({ selected.begin(), selected.end() });
    select ({});
    shown = heardSettings();
}

bool EqDisplay::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && ! selected.empty())
    {
        deleteSelection();
        return true;
    }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAll();
        return true;
    }
    return false;
}

} // namespace eq1
