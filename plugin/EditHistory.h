#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <set>
#include <vector>

namespace eq1
{

// The undo history of the edits made in eq1's editor. An edit is what the editor sets inside parameter
// gestures: only eq1's own editor begins gestures, and a step holds only the parameters gestured in
// it, so Host Automation and the host's own parameter view never become undo steps, even while the
// editor is dragging. Gestures that overlap, or that fall inside a transaction, are one step. Undo
// and redo wait until the edit in progress is finished, and set the parameters inside gestures, so
// the host can record them as automation too. A step also holds what it changed in the state kept
// outside the parameters, such as A/B Compare's side. The history isn't saved: it starts empty, and
// empties when a session is restored.
// Message thread only, except sessionRestored().
class EditHistory final : private juce::AudioProcessorListener
{
public:
    explicit EditHistory (juce::AudioProcessor& processor);
    ~EditHistory() override;

    // State kept outside the parameters that edits can change, read when a step begins and ends and
    // put back by undo and redo.
    struct OutsideState
    {
        virtual ~OutsideState() = default;
        virtual juce::ValueTree capture() const = 0;
        virtual void restore (const juce::ValueTree& state) = 0;
    };
    // The one OutsideState, which must outlive the history or be replaced with nullptr first.
    void track (OutsideState* state) { outside = state; }

    // Holds one undo step open across several gestures, for an edit that changes several parameters
    // one after another: adding a Band, or Spectrum Grab and its drag. Transactions nest.
    void beginTransaction();
    void endTransaction();

    bool canUndo() const { return upToDate() && ! undoStack.empty(); }
    bool canRedo() const { return upToDate() && ! redoStack.empty(); }
    int undoSteps() const { return upToDate() ? static_cast<int> (undoStack.size()) : 0; }

    void undo();
    void redo();

    // A session was restored: the history empties, and the edit in progress is forgotten. From any
    // thread, as hosts restore state from theirs; the message thread catches up on its next call.
    void sessionRestored() { restores.fetch_add (1, std::memory_order_release); }

private:
    // A parameter's normalised value before and after a step.
    struct Change
    {
        int index;
        float before, after;
    };
    struct Step
    {
        std::vector<Change> changes;
        juce::ValueTree outsideBefore, outsideAfter; // invalid when the step left it alone
    };

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override;

    bool upToDate() const { return restoresSeen == restores.load (std::memory_order_acquire); }
    // Empties the history if a session was restored since the last call.
    void catchUp();
    bool editInProgress() const { return openGestures + openTransactions > 0; }
    void openStep();
    void closeStep();
    std::vector<float> values() const;
    void apply (const Step& step, bool forward);

    // Long enough for any session; the oldest steps go first.
    static constexpr size_t maxSteps = 500;

    juce::AudioProcessor& processor;
    int openGestures = 0, openTransactions = 0;
    std::vector<float> valuesBefore; // when the open step began
    juce::ValueTree outsideBefore;    // the outside state when the open step began
    OutsideState* outside = nullptr; // the one tracked, if any
    std::set<int> gestured;          // the parameters gestured in the open step
    std::vector<Step> undoStack, redoStack;
    bool applying = false; // undo or redo is setting the parameters: not an edit
    std::atomic<int> restores { 0 };
    int restoresSeen = 0;

    JUCE_DECLARE_NON_COPYABLE (EditHistory)
};

} // namespace eq1
