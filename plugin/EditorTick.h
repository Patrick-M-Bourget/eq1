#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

// The editor's one tick, which PluginEditor owns. Every component that follows the processor (the
// meters, the Analyzer, what a parameter or a restored session shows) reads it on this tick, as a
// Client, rather than on a timer of its own, and repaints only what changed. It runs at activeHz
// while anything animates or meters (a Client says it is active, or a Tween is easing) and at idleHz
// otherwise. A parameter change, from an edit, Host Automation or a restored session, wakes it to
// activeHz at once: on the message thread the timer is restarted there and then; from another thread
// (the audio thread's automation) the change is only flagged and posted, as JUCE's ParameterAttachment
// posts its own, so nothing is drawn or allocated there. The Tweens (staple/controls/Tween.h) keep
// their own frames: they belong to the Staple kit, which has no editor, and run only while easing.
class EditorTick final : private juce::Timer, private juce::AsyncUpdater, private juce::AudioProcessorListener
{
public:
    static constexpr int activeHz = 60, idleHz = 10;

    class Client;
    // What owns a tick: a Client joins the nearest Host at or above its component.
    struct Host
    {
        virtual ~Host() = default;
        virtual EditorTick& editorTick() = 0;
    };

    explicit EditorTick (juce::AudioProcessor& processor);
    ~EditorTick() override;

    // The rate the next tick comes at: activeHz or idleHz.
    int rateHz() const { return rate; }
    // How many ticks there have been.
    int count() const { return ticks; }
    // A parameter changed: the next tick comes at activeHz (above). Message thread.
    void wake();
    // Puts client on this tick: what a Host does for itself; a Client under a Host joins by itself.
    void add (Client& client);

private:
    void timerCallback() override;
    void handleAsyncUpdate() override { wake(); }
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged (juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override {}
    void runAt (int hz);

    juce::AudioProcessor& processor;
    juce::ListenerList<Client> clients;
    int rate = 0, ticks = 0;
    bool woken = false;    // a parameter changed since the last tick
    double lastTickMs = 0.0; // 0 before the first
};

// What the tick drives: a component that reads the processor each tick and repaints only on a
// change. It joins the Host nearest at or above its component whenever the component's place in the
// hierarchy changes, and leaves when the component goes.
class EditorTick::Client
{
public:
    explicit Client (juce::Component& component);
    virtual ~Client();

    // Each tick, with the seconds since the one before: 1/60 s on the first, and at most 0.25 s after
    // a stall, so a meter's fall doesn't jump.
    virtual void tick (double seconds) = 0;
    // True while it animates or meters, which keeps the tick at activeHz.
    virtual bool isActive() const { return false; }

protected:
    // Asks for the next tick at once (EditorTick::wake): what a pointer event that starts an animation
    // or a wait does, so the first frame isn't up to an idle interval late.
    void wake();

private:
    friend class EditorTick;
    // Joins the Host the component is under now, if any.
    void follow();
    void join (EditorTick* tick);
    // Watches the component's place in the hierarchy, apart from any listener the component is itself.
    struct Watch final : juce::ComponentListener
    {
        explicit Watch (Client& c) : client (c) {}
        void componentParentHierarchyChanged (juce::Component&) override { client.follow(); }
        Client& client;
    };

    juce::Component& component;
    Watch watch { *this };
    EditorTick* source = nullptr; // the tick it is on, while under a Host
};

} // namespace eq1
