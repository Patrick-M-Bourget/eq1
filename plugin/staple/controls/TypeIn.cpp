#include "TypeIn.h"

#include "../Fonts.h"
#include "../Tokens.h"

namespace staple
{

namespace
{
struct TypeInField final : juce::TextEditor
{
    std::function<void()> onEnter, onCancel;

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey && onEnter != nullptr)
        {
            onEnter();
            return true;
        }
        if (key == juce::KeyPress::escapeKey && onCancel != nullptr)
        {
            onCancel();
            return true;
        }
        return juce::TextEditor::keyPressed (key);
    }
};
} // namespace

std::unique_ptr<juce::TextEditor> makeTypeInField (const juce::String& title, const juce::String& text, std::function<void()> onCommit,
                                                   std::function<void()> onCancel)
{
    namespace colour = tokens::colour;
    auto field = std::make_unique<TypeInField>();
    field->onEnter = std::move (onCommit);
    field->onCancel = onCancel;
    field->setTitle (title);
    field->setFont (font (tokens::size::fs3));
    field->setJustification (juce::Justification::centred);
    field->setIndents (6, 2);
    field->setColour (juce::TextEditor::backgroundColourId, colour::fill2);
    field->setColour (juce::TextEditor::outlineColourId, colour::line3);
    field->setColour (juce::TextEditor::focusedOutlineColourId, colour::line3);
    field->setColour (juce::TextEditor::textColourId, colour::text1);
    field->setText (text, false);
    field->onFocusLost = std::move (onCancel);
    return field;
}

void focusTypeInField (juce::TextEditor& field)
{
    if (! field.isShowing())
        return;
    field.grabKeyboardFocus();
    field.selectAll();
}

juce::String closeTypeInField (std::unique_ptr<juce::TextEditor>& field)
{
    if (field == nullptr)
        return {};
    const auto text = field->getText();
    std::shared_ptr<juce::TextEditor> closing (field.release());
    if (auto* parent = closing->getParentComponent())
        parent->removeChildComponent (closing.get());
    juce::MessageManager::callAsync ([closing] {});
    return text;
}

} // namespace staple
