#include "ABCompare.h"

#include "PresetSettings.h"

#include <utility>

namespace eq1
{

namespace
{
const juce::Identifier activeProperty { "active" }, sideType { "Side" }, idProperty { "id" }, valueProperty { "value" },
    loadedPresetType { "LoadedPreset" }, sideProperty { "side" }, nameProperty { "name" };

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
    return state.active;
}

ABCompare::State ABCompare::current() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return state;
}

void ABCompare::set (State next)
{
    const juce::SpinLock::ScopedLockType hold (lock);
    state = std::move (next);
    ++changes;
}

void ABCompare::putOnParameters (const juce::ValueTree& settings, State next)
{
    {
        const juce::SpinLock::ScopedLockType hold (lock);
        state = std::move (next);
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
    auto next = current();
    const auto target = std::exchange (next.other, capturePresetSettings (parameters, sideType));
    next.active = side;
    if (target.isValid())
        putOnParameters (target, std::move (next));
    else
    {
        // B, still a copy of A, is selected for the first time: it takes A's Loaded Preset too.
        next.loadedOn (CompareSide::B) = next.loadedOn (CompareSide::A);
        loadedFrom[indexOf (CompareSide::B)] = loadedFrom[indexOf (CompareSide::A)];
        set (std::move (next));
    }
    history.endTransaction();
}

void ABCompare::copyToOther()
{
    history.beginTransaction();
    auto next = current();
    const auto to = next.active == CompareSide::A ? CompareSide::B : CompareSide::A;
    next.loadedOn (to) = next.loadedOn (next.active);
    loadedFrom[indexOf (to)] = loadedFrom[indexOf (next.active)];
    next.other = capturePresetSettings (parameters, sideType);
    set (std::move (next));
    history.endTransaction();
}

void ABCompare::loadPreset (const juce::ValueTree& preset, const juce::String& name, const juce::String& folder)
{
    loadedFrom[indexOf (side())] = { name, folder };
    const auto loaded = loadedPresetOf (preset, name);
    history.beginTransaction();
    auto next = current();
    next.loadedOn (next.active) = loaded;
    putOnParameters (loaded, std::move (next));
    history.endTransaction();
}

void ABCompare::presetSaved (const juce::ValueTree& preset, const juce::String& name, const juce::String& folder)
{
    loadedFrom[indexOf (side())] = { name, folder };
    history.beginTransaction();
    auto next = current();
    next.loadedOn (next.active) = loadedPresetOf (preset, name);
    set (std::move (next));
    history.endTransaction();
}

juce::ValueTree ABCompare::loadedPresetOf (const juce::ValueTree& preset, const juce::String& name)
{
    return presetSettingsAsLoaded (parameters, preset, loadedPresetType).setProperty (nameProperty, name, nullptr);
}

juce::String ABCompare::loadedPresetName() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return state.loadedOn (state.active).getProperty (nameProperty).toString();
}

juce::String ABCompare::loadedPresetFolder() const
{
    const auto& from = loadedFrom[indexOf (side())];
    return from.name == loadedPresetName() ? from.folder : juce::String();
}

bool ABCompare::isModified() const
{
    const auto s = current();
    const auto& loaded = s.loadedOn (s.active);
    return loaded.isValid() && ! holdsPresetSettings (parameters, loaded);
}

juce::ValueTree ABCompare::treeOf (const State& s)
{
    auto tree = juce::ValueTree (stateType).setProperty (activeProperty, nameOf (s.active), nullptr);
    if (s.other.isValid())
        tree.appendChild (s.other.createCopy(), nullptr);
    for (const auto side : { CompareSide::A, CompareSide::B })
        if (const auto& loaded = s.loadedOn (side); loaded.isValid())
            tree.appendChild (loaded.createCopy().setProperty (sideProperty, nameOf (side), nullptr), nullptr);
    return tree;
}

juce::ValueTree ABCompare::capture() const
{
    const juce::SpinLock::ScopedLockType hold (lock);
    return treeOf (state);
}

juce::ValueTree ABCompare::savedState() const
{
    // Copies the parameters again whenever a side began to be put on them while it copied. A side
    // already arriving may finish meanwhile: it is whole in comingIn either way.
    for (;;)
    {
        int changesBefore;
        State kept;
        juce::ValueTree comingIn;
        {
            const juce::SpinLock::ScopedLockType hold (lock);
            changesBefore = changes;
            kept = state;
            comingIn = arriving;
        }
        auto saved = parameters.copyState();
        {
            const juce::SpinLock::ScopedLockType hold (lock);
            const int changed = changes - changesBefore;
            if (changed > (comingIn.isValid() ? 1 : 0))
                continue;
        }
        for (const auto& setting : comingIn)
            saved.getChildWithProperty (idProperty, setting.getProperty (idProperty))
                .setProperty (valueProperty, setting.getProperty (valueProperty), nullptr);
        saved.appendChild (treeOf (kept), nullptr);
        return saved;
    }
}

void ABCompare::restore (const juce::ValueTree& saved)
{
    State next;
    next.active = saved.getProperty (activeProperty).toString() == nameOf (CompareSide::B) ? CompareSide::B : CompareSide::A;
    if (const auto otherSaved = saved.getChildWithName (sideType); otherSaved.isValid())
        next.other = otherSaved.createCopy();
    for (const auto side : { CompareSide::A, CompareSide::B })
        if (const auto loaded = saved.getChildWithProperty (sideProperty, nameOf (side)); loaded.hasType (loadedPresetType))
            next.loadedOn (side) = loaded.createCopy();
    set (std::move (next));
}

juce::ValueTree ABCompare::initialState()
{
    return treeOf ({});
}

} // namespace eq1
