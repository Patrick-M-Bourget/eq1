#include "ABCompare.h"

#include "PresetSettings.h"

namespace eq1
{

namespace
{
const juce::Identifier activeProperty { "active" }, sideType { "Side" }, idProperty { "id" }, valueProperty { "value" };

juce::String nameOf (CompareSide side) { return side == CompareSide::A ? "A" : "B"; }
} // namespace

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
    ++changes;
}

void ABCompare::putOnParameters (const juce::ValueTree& settings, CompareSide side, juce::ValueTree otherSide)
{
    {
        const juce::SpinLock::ScopedLockType hold (lock);
        active = side;
        other = std::move (otherSide);
        arriving = settings;
        ++changes;
    }
    applyPresetSettings (parameters, settings);
    const juce::SpinLock::ScopedLockType hold (lock);
    arriving = {};
    ++changes;
}

void ABCompare::select (CompareSide side)
{
    if (side == this->side())
        return;
    history.beginTransaction();
    auto leaving = capturePresetSettings (parameters, sideType);
    if (const auto target = otherSide(); target.isValid())
        putOnParameters (target, side, std::move (leaving));
    else
        set (side, std::move (leaving));
    history.endTransaction();
}

void ABCompare::copyAToB()
{
    history.beginTransaction();
    if (side() == CompareSide::A)
        set (CompareSide::A, capturePresetSettings (parameters, sideType));
    else if (auto a = otherSide(); a.isValid())
        putOnParameters (a, CompareSide::B, a);
    history.endTransaction();
}

juce::ValueTree ABCompare::treeOf (CompareSide selected, const juce::ValueTree& unselected)
{
    auto state = juce::ValueTree (stateType).setProperty (activeProperty, nameOf (selected), nullptr);
    if (unselected.isValid())
        state.appendChild (unselected.createCopy(), nullptr);
    return state;
}

juce::ValueTree ABCompare::capture() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return treeOf (active, other);
}

juce::ValueTree ABCompare::savedState() const
{
    // Copies the parameters again whenever a side began to be put on them while it copied. A side
    // already arriving may finish meanwhile: it is whole in comingIn either way.
    for (;;)
    {
        int changesBefore;
        CompareSide selected;
        juce::ValueTree unselected, comingIn;
        {
            const juce::SpinLock::ScopedLockType hold (lock);
            changesBefore = changes;
            selected = active;
            unselected = other;
            comingIn = arriving;
        }
        auto state = parameters.copyState();
        {
            const juce::SpinLock::ScopedLockType hold (lock);
            const int changed = changes - changesBefore;
            if (changed > (comingIn.isValid() ? 1 : 0))
                continue;
        }
        for (const auto& setting : comingIn)
            state.getChildWithProperty (idProperty, setting.getProperty (idProperty))
                .setProperty (valueProperty, setting.getProperty (valueProperty), nullptr);
        state.appendChild (treeOf (selected, unselected), nullptr);
        return state;
    }
}

void ABCompare::restore (const juce::ValueTree& saved)
{
    const auto otherSaved = saved.getChildWithName (sideType);
    set (saved.getProperty (activeProperty).toString() == nameOf (CompareSide::B) ? CompareSide::B : CompareSide::A,
         otherSaved.isValid() ? otherSaved.createCopy() : juce::ValueTree());
}

juce::ValueTree ABCompare::initialState()
{
    return treeOf (CompareSide::A, {});
}

} // namespace eq1
