#include "ABCompare.h"

#include "PresetSettings.h"

namespace eq1
{

namespace
{
const juce::Identifier compareType { "ABCompare" }, activeProperty { "active" }, sideType { "Side" };
}

ABCompare::ABCompare (juce::AudioProcessorValueTreeState& p, EditHistory& h) : parameters (p), history (h)
{
    history.track (this);
}

ABCompare::~ABCompare()
{
    history.track (nullptr);
}

void ABCompare::select (CompareSide side)
{
    if (side == active)
        return;
    history.beginTransaction();
    auto leaving = capturePresetSettings (parameters, sideType);
    if (other.isValid())
        applyPresetSettings (parameters, other);
    other = leaving;
    active = side;
    history.endTransaction();
}

void ABCompare::copyAToB()
{
    history.beginTransaction();
    if (active == CompareSide::A)
        other = capturePresetSettings (parameters, sideType);
    else if (other.isValid())
        applyPresetSettings (parameters, other);
    history.endTransaction();
}

juce::ValueTree ABCompare::capture() const
{
    juce::ValueTree state (compareType);
    state.setProperty (activeProperty, active == CompareSide::A ? "A" : "B", nullptr);
    if (other.isValid())
        state.appendChild (other.createCopy(), nullptr);
    return state;
}

void ABCompare::restore (const juce::ValueTree& state)
{
    active = state.getProperty (activeProperty).toString() == "B" ? CompareSide::B : CompareSide::A;
    const auto saved = state.getChildWithName (sideType);
    other = saved.isValid() ? saved.createCopy() : juce::ValueTree();
}

} // namespace eq1
