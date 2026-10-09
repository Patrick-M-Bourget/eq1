#include "BandEditing.h"

#include "Parameters.h"

namespace eq1
{

BandEditing::BandEditing (juce::AudioProcessorValueTreeState& parametersToEdit)
    : parameters (parametersToEdit), output (parameters::OutputValues::of (parametersToEdit))
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
    for (int slot = 1; slot <= numBandSlots; ++slot)
        if (! band (slot).inUse)
            return false;
    return true;
}

std::optional<int> BandEditing::add (double frequency, double gain)
{
    for (int slot = 1; slot <= numBandSlots; ++slot)
    {
        if (band (slot).inUse)
            continue;
        // The slot's settings first, so the audio never plays the Band with what the slot held before.
        const BandSettings defaults;
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
        return slot;
    }
    return std::nullopt;
}

std::optional<int> BandEditing::grab (double frequency)
{
    const auto slot = add (frequency, 0.0);
    if (slot)
        beginDrag ({ *slot });
    return slot;
}

void BandEditing::deleteBand (int slot)
{
    set (parameters::inUseId (slot), 0.0);
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
}

void BandEditing::scaleQ (int slot, double factor)
{
    set (parameters::qId (slot), band (slot).q * factor);
}

void BandEditing::setShape (int slot, Shape shape)
{
    set (parameters::shapeId (slot), static_cast<double> (shape));
}

} // namespace eq1
