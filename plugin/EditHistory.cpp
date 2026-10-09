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

void EditHistory::open()
{
    if (openGestures + openTransactions == 1)
        valuesBefore = values();
}

void EditHistory::close()
{
    if (openGestures + openTransactions != 0)
        return;
    const auto valuesAfter = values();
    Step step;
    for (size_t i = 0; i < valuesAfter.size() && i < valuesBefore.size(); ++i)
        if (! juce::exactlyEqual (valuesBefore[i], valuesAfter[i]))
            step.push_back ({ static_cast<int> (i), valuesBefore[i], valuesAfter[i] });
    if (step.empty())
        return;
    undoStack.push_back (std::move (step));
    if (undoStack.size() > maxSteps)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void EditHistory::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int)
{
    if (applying)
        return;
    ++openGestures;
    open();
}

void EditHistory::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int)
{
    // An end without its begin (one begun before the history was listening) changes nothing.
    if (applying || openGestures == 0)
        return;
    --openGestures;
    close();
}

void EditHistory::beginTransaction()
{
    ++openTransactions;
    open();
}

void EditHistory::endTransaction()
{
    if (openTransactions == 0)
        return;
    --openTransactions;
    close();
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
    if (undoStack.empty())
        return;
    apply (undoStack.back(), false);
    redoStack.push_back (std::move (undoStack.back()));
    undoStack.pop_back();
}

void EditHistory::redo()
{
    if (redoStack.empty())
        return;
    apply (redoStack.back(), true);
    undoStack.push_back (std::move (redoStack.back()));
    redoStack.pop_back();
}

void EditHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
}

} // namespace eq1
