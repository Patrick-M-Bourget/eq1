#include "BandEditing.h"

#include "Parameters.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <utility>

namespace eq1
{

BandEditing::BandEditing (juce::AudioProcessorValueTreeState& parametersToEdit, EditHistory& editHistory)
    : parameters (parametersToEdit), history (editHistory), output (parameters::OutputValues::of (parametersToEdit))
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
        slots[static_cast<size_t> (slot - 1)] = parameters::SlotValues::of (parameters, slot);
}

BandEditing::~BandEditing()
{
    endDrag();
}

juce::RangedAudioParameter& BandEditing::parameter (const juce::String& id) const
{
    auto* found = parameters.getParameter (id);
    jassert (found != nullptr);
    return *found;
}

void BandEditing::setWithinGesture (const juce::String& id, double value)
{
    auto& p = parameter (id);
    p.setValueNotifyingHost (p.convertTo0to1 (p.getNormalisableRange().snapToLegalValue (static_cast<float> (value))));
}

void BandEditing::set (const juce::String& id, double value)
{
    auto& p = parameter (id);
    p.beginChangeGesture();
    setWithinGesture (id, value);
    p.endChangeGesture();
}

BandSettings BandEditing::band (int slot) const
{
    return slots[static_cast<size_t> (slot - 1)].read();
}

Settings BandEditing::settings() const
{
    Settings result;
    for (int slot = 1; slot <= numBandSlots; ++slot)
        result.bands[static_cast<size_t> (slot - 1)] = band (slot);
    output.readInto (result);
    return result;
}

double BandEditing::storedGain (double heard) const
{
    Settings whole;
    output.readInto (whole);
    const double gainScale = whole.gainScale;
    return gainScale > 0.0 ? heard / gainScale : 0.0;
}

bool BandEditing::isFull() const
{
    return freeSlots() == 0;
}

int BandEditing::freeSlots() const
{
    int free = 0;
    for (int slot = 1; slot <= numBandSlots; ++slot)
        if (! band (slot).inUse)
            ++free;
    return free;
}

std::optional<int> BandEditing::add (double frequency, double gain)
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        if (band (slot).inUse)
            continue;
        // The slot's settings first, so the audio never plays the Band with what the slot held before.
        const BandSettings defaults;
        history.beginTransaction();
        set (parameters::shapeId (slot), static_cast<double> (defaults.shape));
        set (parameters::frequencyId (slot), frequency);
        set (parameters::gainId (slot), storedGain (gain));
        set (parameters::qId (slot), defaults.q);
        set (parameters::slopeId (slot), defaults.slope);
        set (parameters::brickwallId (slot), static_cast<double> (defaults.brickwall));
        set (parameters::placementId (slot), static_cast<double> (defaults.placement));
        set (parameters::dynamicRangeId (slot), defaults.dynamicRange);
        set (parameters::thresholdId (slot), defaults.threshold);
        set (parameters::thresholdAutoId (slot), static_cast<double> (defaults.thresholdAuto));
        set (parameters::attackId (slot), defaults.attack);
        set (parameters::releaseId (slot), defaults.release);
        set (parameters::dynamicsBypassId (slot), static_cast<double> (defaults.dynamicsBypass));
        set (parameters::bypassId (slot), 0.0);
        set (parameters::inUseId (slot), 1.0);
        history.endTransaction();
        return slot;
    }
    return std::nullopt;
}

std::optional<int> BandEditing::grab (double frequency)
{
    history.beginTransaction();
    const auto slot = add (frequency, 0.0);
    if (! slot)
    {
        history.endTransaction();
        return slot;
    }
    beginDrag ({ *slot });
    grabbing = true;
    return slot;
}

void BandEditing::deleteBand (int slot)
{
    set (parameters::inUseId (slot), 0.0);
}

void BandEditing::deleteBands (const std::vector<int>& slotsToDelete)
{
    history.beginTransaction();
    for (int slot : slotsToDelete)
        deleteBand (slot);
    history.endTransaction();
}

void BandEditing::beginDrag (std::vector<int> slotsToDrag)
{
    endDrag();
    for (int slot : slotsToDrag)
    {
        const auto settings = band (slot);
        dragged.push_back ({ slot, settings.frequency, settings.gain, hasGain (settings.shape) });
        parameter (parameters::frequencyId (slot)).beginChangeGesture();
        if (dragged.back().movesGain)
            parameter (parameters::gainId (slot)).beginChangeGesture();
    }
}

void BandEditing::dragBy (double frequencyRatio, double gainOffset)
{
    if (dragged.empty())
        return;
    gainOffset = storedGain (gainOffset);
    // The furthest the group can go before one of its Bands leaves a range.
    const auto& frequencyRange = parameter (parameters::frequencyId (1)).getNormalisableRange();
    const auto& gainRange = parameter (parameters::gainId (1)).getNormalisableRange();
    for (const auto& d : dragged)
    {
        frequencyRatio = juce::jlimit (frequencyRange.start / d.frequency, frequencyRange.end / d.frequency, frequencyRatio);
        if (d.movesGain)
            gainOffset = juce::jlimit (gainRange.start - d.gain, gainRange.end - d.gain, gainOffset);
    }
    for (const auto& d : dragged)
    {
        setWithinGesture (parameters::frequencyId (d.slot), d.frequency * frequencyRatio);
        if (d.movesGain)
            setWithinGesture (parameters::gainId (d.slot), d.gain + gainOffset);
    }
}

void BandEditing::endDrag()
{
    for (const auto& d : dragged)
    {
        parameter (parameters::frequencyId (d.slot)).endChangeGesture();
        if (d.movesGain)
            parameter (parameters::gainId (d.slot)).endChangeGesture();
    }
    dragged.clear();
    if (std::exchange (grabbing, false))
        history.endTransaction();
}

void BandEditing::scaleQ (int slot, double factor)
{
    set (parameters::qId (slot), band (slot).q * factor);
}

void BandEditing::scaleQ (const std::vector<int>& slotsToScale, double factor)
{
    history.beginTransaction();
    for (int slot : slotsToScale)
        scaleQ (slot, factor);
    history.endTransaction();
}

void BandEditing::setShape (int slot, Shape shape)
{
    set (parameters::shapeId (slot), static_cast<double> (shape));
}

void BandEditing::editEach (const std::vector<int>& slotsToEdit, const std::function<void (int, const BandSettings&)>& edit)
{
    history.beginTransaction();
    for (int slot : slotsToEdit)
        edit (slot, band (slot));
    history.endTransaction();
}

void BandEditing::setBypass (const std::vector<int>& slotsToEdit, bool bypass)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings&) { set (parameters::bypassId (slot), bypass ? 1.0 : 0.0); });
}

void BandEditing::invertGain (const std::vector<int>& slotsToEdit)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings& settings) {
        if (! hasGain (settings.shape))
            return;
        set (parameters::gainId (slot), -settings.gain);
        set (parameters::dynamicRangeId (slot), -settings.dynamicRange);
    });
}

void BandEditing::clearDynamics (const std::vector<int>& slotsToEdit)
{
    const BandSettings defaults;
    editEach (slotsToEdit, [&] (int slot, const BandSettings& settings) {
        if (! hasDynamics (settings.shape))
            return;
        set (parameters::dynamicRangeId (slot), defaults.dynamicRange);
        set (parameters::thresholdId (slot), defaults.threshold);
        set (parameters::thresholdAutoId (slot), static_cast<double> (defaults.thresholdAuto));
        set (parameters::attackId (slot), defaults.attack);
        set (parameters::releaseId (slot), defaults.release);
        set (parameters::dynamicsBypassId (slot), static_cast<double> (defaults.dynamicsBypass));
        set (parameters::detectionSourceId (slot), static_cast<double> (defaults.detectionSource));
        set (parameters::detectionRangeId (slot), static_cast<double> (defaults.detectionRange));
        set (parameters::detectionLowId (slot), defaults.detectionLow);
        set (parameters::detectionHighId (slot), defaults.detectionHigh);
    });
}

void BandEditing::setShape (const std::vector<int>& slotsToEdit, Shape shape)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings&) { setShape (slot, shape); });
}

void BandEditing::setSlope (const std::vector<int>& slotsToEdit, double slope)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings& settings) {
        if (! hasSlope (settings.shape))
            return;
        set (parameters::slopeId (slot), slope);
        if (isCut (settings.shape))
            set (parameters::brickwallId (slot), 0.0);
    });
}

void BandEditing::setBrickwall (const std::vector<int>& slotsToEdit)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings& settings) {
        if (isCut (settings.shape))
            set (parameters::brickwallId (slot), 1.0);
    });
}

void BandEditing::setPlacement (const std::vector<int>& slotsToEdit, StereoPlacement placement)
{
    editEach (slotsToEdit, [&] (int slot, const BandSettings&) { set (parameters::placementId (slot), static_cast<double> (placement)); });
}

std::vector<int> BandEditing::split (const std::vector<int>& slotsToSplit)
{
    std::vector<int> stereo;
    for (int slot : slotsToSplit)
        if (const auto settings = band (slot); settings.inUse && settings.placement == StereoPlacement::Stereo)
            stereo.push_back (slot);
    std::sort (stereo.begin(), stereo.end(), [this] (int a, int b) { return std::pair (band (a).frequency, a) < std::pair (band (b).frequency, b); });

    // Every setting a Right half copies from its Stereo Band, by normalised value so it is exact.
    using IdOf = juce::String (*) (int);
    static constexpr std::array<IdOf, 17> copied {
        parameters::frequencyId,       parameters::gainId,           parameters::qId,             parameters::bypassId,
        parameters::shapeId,           parameters::slopeId,          parameters::brickwallId,     parameters::dynamicRangeId,
        parameters::thresholdId,       parameters::thresholdAutoId,  parameters::attackId,        parameters::releaseId,
        parameters::dynamicsBypassId,  parameters::detectionSourceId, parameters::detectionRangeId, parameters::detectionLowId,
        parameters::detectionHighId,
    };
    std::vector<int> halves;
    history.beginTransaction();
    for (int slot : stereo)
    {
        int right = 1;
        while (right <= numBandSlots && band (right).inUse)
            ++right;
        if (right > numBandSlots)
            break;
        // The new Band's settings first, so the audio never plays it with what the slot held before.
        for (auto idOf : copied)
        {
            auto& to = parameter (idOf (right));
            to.beginChangeGesture();
            to.setValueNotifyingHost (parameter (idOf (slot)).getValue());
            to.endChangeGesture();
        }
        set (parameters::placementId (right), static_cast<double> (StereoPlacement::Right));
        set (parameters::placementId (slot), static_cast<double> (StereoPlacement::Left));
        set (parameters::inUseId (right), 1.0);
        halves.insert (halves.end(), { slot, right });
    }
    history.endTransaction();
    std::sort (halves.begin(), halves.end());
    return halves;
}

std::vector<int> BandEditing::paste (const std::vector<BandSettings>& bands)
{
    std::vector<int> free;
    for (int slot = 1; slot <= numBandSlots; ++slot)
        if (! band (slot).inUse)
            free.push_back (slot);
    // The lowest-Frequency Bands that fit, then back in the order given.
    std::vector<size_t> chosen (bands.size());
    std::iota (chosen.begin(), chosen.end(), size_t { 0 });
    std::stable_sort (chosen.begin(), chosen.end(), [&bands] (size_t a, size_t b) { return bands[a].frequency < bands[b].frequency; });
    chosen.resize (std::min (chosen.size(), free.size()));
    std::sort (chosen.begin(), chosen.end());

    std::vector<int> pasted;
    history.beginTransaction();
    for (size_t i : chosen)
    {
        const auto& pastedBand = bands[i];
        const int slot = free[pasted.size()];
        // The slot's settings first, so the audio never plays the Band with what the slot held before.
        set (parameters::bypassId (slot), static_cast<double> (pastedBand.bypass));
        set (parameters::shapeId (slot), static_cast<double> (pastedBand.shape));
        set (parameters::frequencyId (slot), pastedBand.frequency);
        set (parameters::gainId (slot), pastedBand.gain);
        set (parameters::qId (slot), pastedBand.q);
        set (parameters::slopeId (slot), pastedBand.slope);
        set (parameters::brickwallId (slot), static_cast<double> (pastedBand.brickwall));
        set (parameters::placementId (slot), static_cast<double> (pastedBand.placement));
        set (parameters::dynamicRangeId (slot), pastedBand.dynamicRange);
        set (parameters::thresholdId (slot), pastedBand.threshold);
        set (parameters::thresholdAutoId (slot), static_cast<double> (pastedBand.thresholdAuto));
        set (parameters::attackId (slot), pastedBand.attack);
        set (parameters::releaseId (slot), pastedBand.release);
        set (parameters::dynamicsBypassId (slot), static_cast<double> (pastedBand.dynamicsBypass));
        set (parameters::detectionSourceId (slot), static_cast<double> (pastedBand.detectionSource));
        set (parameters::detectionRangeId (slot), static_cast<double> (pastedBand.detectionRange));
        set (parameters::detectionLowId (slot), pastedBand.detectionLow);
        set (parameters::detectionHighId (slot), pastedBand.detectionHigh);
        set (parameters::inUseId (slot), 1.0);
        pasted.push_back (slot);
    }
    history.endTransaction();
    return pasted;
}

} // namespace eq1
