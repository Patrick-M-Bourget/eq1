#include "EditorTick.h"

#include "staple/controls/Tween.h"

namespace eq1
{

EditorTick::EditorTick (juce::AudioProcessor& p) : processor (p)
{
    processor.addListener (this);
    runAt (activeHz); // the first frames show what the editor opened with
}

EditorTick::~EditorTick()
{
    processor.removeListener (this);
    cancelPendingUpdate();
    stopTimer();
    clients.call ([] (Client& client) { client.source = nullptr; });
}

void EditorTick::add (Client& client)
{
    client.join (this);
}

void EditorTick::wake()
{
    woken = true;
    runAt (activeHz);
}

void EditorTick::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        wake();
    else
        triggerAsyncUpdate();
}

void EditorTick::runAt (int hz)
{
    if (hz == rate)
        return;
    rate = hz;
    startTimerHz (hz);
}

void EditorTick::timerCallback()
{
    ++ticks;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double seconds = lastTickMs <= 0.0 ? 1.0 / activeHz : juce::jlimit (0.001, 0.25, (now - lastTickMs) / 1000.0);
    lastTickMs = now;
    // A change a Client makes while ticking is one the next tick shows.
    woken = false;
    clients.call ([seconds] (Client& client) { client.tick (seconds); });
    bool active = woken || staple::Tween::numberEasing() > 0;
    clients.call ([&active] (Client& client) { active = active || client.isActive(); });
    runAt (active ? activeHz : idleHz);
}

//==============================================================================
EditorTick::Client::Client (juce::Component& c) : component (c)
{
    component.addComponentListener (&watch);
}

EditorTick::Client::~Client()
{
    component.removeComponentListener (&watch);
    join (nullptr);
}

void EditorTick::Client::follow()
{
    auto* host = dynamic_cast<Host*> (&component);
    if (host == nullptr)
        host = component.findParentComponentOfClass<Host>();
    join (host != nullptr ? &host->editorTick() : nullptr);
}

void EditorTick::Client::join (EditorTick* tick)
{
    if (tick == source)
        return;
    if (source != nullptr)
        source->clients.remove (this);
    source = tick;
    if (source != nullptr)
        source->clients.add (this);
}

} // namespace eq1
