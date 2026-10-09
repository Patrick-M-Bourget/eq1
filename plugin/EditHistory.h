#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace eq1
{

// The undo history of the edits made in eq1's editor. An edit is what happens inside a parameter
// gesture: only eq1's own editor begins gestures, so Host Automation and the host's own parameter view
// never become undo steps. Gestures that overlap, or that fall inside a transaction, are one step.
// Undo and redo set the parameters inside gestures, so the host can record them as automation too.
// The history isn't saved: it starts empty, and is cleared when a session is restored.
// Message thread only.
class EditHistory final : private juce::AudioProcessorListener
{
public:
    explicit EditHistory (juce::AudioProcessor& processor);
    ~EditHistory() override;

    // Holds one undo step open across several gestures, for an edit that changes several parameters
    // one after another: adding a Band, or Spectrum Grab and its drag. Transactions nest.
    void beginTransaction();
    void endTransaction();

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    int undoSteps() const { return static_cast<int> (undoStack.size()); }

    void undo();
    void redo();
    void clear();

private:
    // A parameter's normalised value before and after a step.
    struct Change
    {
        int index;
        float before, after;
    };
    using Step = std::vector<Change>;

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override;

    void open();
    void close();
    std::vector<float> values() const;
    void apply (const Step& step, bool forward);

    // Long enough for any session; the oldest steps go first.
    static constexpr size_t maxSteps = 500;

    juce::AudioProcessor& processor;
    int openGestures = 0, openTransactions = 0;
    std::vector<float> valuesBefore; // when the open step began
    std::vector<Step> undoStack, redoStack;
    bool applying = false; // undo or redo is setting the parameters: not an edit

    JUCE_DECLARE_NON_COPYABLE (EditHistory)
};

} // namespace eq1
