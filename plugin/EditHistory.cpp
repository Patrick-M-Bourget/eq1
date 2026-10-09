#include "EditHistory.h"

namespace eq1
{

EditHistory::EditHistory (juce::AudioProcessor& p) : processor (p)
{
    processor.addListener (this);
}

EditHistory::~EditHistory()
{
    processor.removeListener (this);
}

std::vector<float> EditHistory::values() const
{
    std::vector<float> result;
    for (auto* parameter : processor.getParameters())
        result.push_back (parameter->getValue());
    return result;
}

void EditHistory::catchUp()
{
    const int restored = restores.load (std::memory_order_acquire);
    if (restoresSeen == restored)
        return;
    restoresSeen = restored;
    undoStack.clear();
    redoStack.clear();
    openGestures = openTransactions = 0;
    gestured.clear();
}

void EditHistory::openStep()
{
    if (openGestures + openTransactions != 1)
        return;
    valuesBefore = values();
    gestured.clear();
}

void EditHistory::closeStep()
{
    if (editInProgress())
        return;
    const auto valuesAfter = values();
    Step step;
    for (int index : gestured)
    {
        const auto i = static_cast<size_t> (index);
        if (i < valuesAfter.size() && i < valuesBefore.size() && ! juce::exactlyEqual (valuesBefore[i], valuesAfter[i]))
            step.push_back ({ index, valuesBefore[i], valuesAfter[i] });
    }
    gestured.clear();
    if (step.empty())
        return;
    undoStack.push_back (std::move (step));
    if (undoStack.size() > maxSteps)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void EditHistory::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index)
{
    catchUp();
    if (applying)
        return;
    ++openGestures;
    openStep();
    gestured.insert (index);
}

void EditHistory::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int)
{
    catchUp();
    // An end without its begin (one begun before the history was listening, or before a session was
    // restored) changes nothing.
    if (applying || openGestures == 0)
        return;
    --openGestures;
    closeStep();
}

void EditHistory::beginTransaction()
{
    catchUp();
    ++openTransactions;
    openStep();
}

void EditHistory::endTransaction()
{
    catchUp();
    if (openTransactions == 0)
        return;
    --openTransactions;
    closeStep();
}

void EditHistory::apply (const Step& step, bool forward)
{
    const juce::ScopedValueSetter<bool> notAnEdit (applying, true);
    const auto& parameters = processor.getParameters();
    for (const auto& change : step)
        parameters[change.index]->beginChangeGesture();
    for (const auto& change : step)
        parameters[change.index]->setValueNotifyingHost (forward ? change.after : change.before);
    for (const auto& change : step)
        parameters[change.index]->endChangeGesture();
}

void EditHistory::undo()
{
    catchUp();
    if (editInProgress() || undoStack.empty())
        return;
    apply (undoStack.back(), false);
    redoStack.push_back (std::move (undoStack.back()));
    undoStack.pop_back();
}

void EditHistory::redo()
{
    catchUp();
    if (editInProgress() || redoStack.empty())
        return;
    apply (redoStack.back(), true);
    undoStack.push_back (std::move (redoStack.back()));
    redoStack.pop_back();
}

} // namespace eq1
