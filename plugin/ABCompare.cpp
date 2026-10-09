#include "ABCompare.h"

#include "PresetSettings.h"

namespace eq1
{

namespace
{
const juce::Identifier activeProperty { "active" }, sideType { "Side" };
}

const juce::Identifier ABCompare::stateType { "ABCompare" };

ABCompare::ABCompare (juce::AudioProcessorValueTreeState& p, EditHistory& h) : parameters (p), history (h)
{
    history.track (this);
}

ABCompare::~ABCompare()
{
    history.track (nullptr);
}

CompareSide ABCompare::side() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return active;
}

juce::ValueTree ABCompare::otherSide() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return other;
}

void ABCompare::set (CompareSide side, juce::ValueTree otherSide)
{
    const juce::SpinLock::ScopedLockType hold (lock);
    active = side;
    other = std::move (otherSide);
}

void ABCompare::select (CompareSide side)
{
    if (side == this->side())
        return;
    history.beginTransaction();
    auto leaving = capturePresetSettings (parameters, sideType);
    if (const auto target = otherSide(); target.isValid())
        applyPresetSettings (parameters, target);
    set (side, std::move (leaving));
    history.endTransaction();
}

void ABCompare::copyAToB()
{
    history.beginTransaction();
    if (side() == CompareSide::A)
        set (CompareSide::A, capturePresetSettings (parameters, sideType));
    else if (const auto a = otherSide(); a.isValid())
        applyPresetSettings (parameters, a);
    history.endTransaction();
}

juce::ValueTree ABCompare::capture() const
{
    CompareSide selected;
    juce::ValueTree unselected;
    {
        const juce::SpinLock::ScopedLockType hold (lock);
        selected = active;
        unselected = other;
    }
    juce::ValueTree state (stateType);
    state.setProperty (activeProperty, selected == CompareSide::A ? "A" : "B", nullptr);
    if (unselected.isValid())
        state.appendChild (unselected.createCopy(), nullptr);
    return state;
}

void ABCompare::restore (const juce::ValueTree& state)
{
    const auto saved = state.getChildWithName (sideType);
    set (state.getProperty (activeProperty).toString() == "B" ? CompareSide::B : CompareSide::A,
         saved.isValid() ? saved.createCopy() : juce::ValueTree());
}

} // namespace eq1
