#include "KnobTooltip.h"

#include "../Fonts.h"
#include "../Tokens.h"
#include "Knob.h"
#include "Overlay.h"

namespace staple
{

namespace
{
namespace colour = tokens::colour;
namespace size = tokens::size;

constexpr int minimumWidth = 86, paddingTop = 5, paddingSide = 10, paddingBottom = 6;
constexpr int titleLine = 14, valueLine = 18, gap = 6;
constexpr int fieldWidth = 74, fieldHeight = 20;
} // namespace

KnobTooltip::KnobTooltip (Knob& k) : knob (k)
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::IBeamCursor);
    setAccessible (false);
}

KnobTooltip::~KnobTooltip() = default;

void KnobTooltip::refresh()
{
    title = knob.tooltipTitle();
    value = knob.tooltipValue();
    const float textWidth = std::max (juce::GlyphArrangement::getStringWidth (font (size::fs2), title),
                                      juce::GlyphArrangement::getStringWidth (font (size::fs4), value));
    const int width = std::max (minimumWidth, static_cast<int> (std::ceil (textWidth)) + 2 * paddingSide);
    const int height = paddingTop + titleLine + valueLine + paddingBottom;

    if (auto* layer = getParentComponent())
    {
        const float r = knob.getFaceRadius();
        const auto face = juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (knob.getFaceCentre());
        const auto area = layer->getLocalArea (&knob, face).toNearestInt();
        auto bounds = juce::Rectangle<int> (width, height).withCentre ({ area.getCentreX(), 0 });
        bounds.setY (area.getY() - gap - height);
        if (bounds.getY() < 0)
            bounds.setY (area.getBottom() + gap);
        bounds.setX (juce::jlimit (0, std::max (0, layer->getWidth() - width), bounds.getX()));
        setBounds (bounds);
    }
    repaint();
}

void KnobTooltip::appear()
{
    toFront (false);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, tokens::motion::dur1Ms);
    setVisible (true);
}

void KnobTooltip::disappear()
{
    if (isEditing())
        stopEditing (false);
    juce::Desktop::getInstance().getAnimator().cancelAnimation (this, false);
    setVisible (false);
}

void KnobTooltip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawSoftShadow (g, bounds, size::r2, tokens::shadow::shadow1);
    g.setColour (colour::menu);
    g.fillRoundedRectangle (bounds, size::r2);
    g.setColour (colour::line2);
    g.drawRoundedRectangle (bounds.reduced (0.5f), size::r2, 1.0f);

    auto text = getLocalBounds().withTrimmedTop (paddingTop).reduced (paddingSide, 0);
    g.setFont (font (size::fs2));
    g.setColour (colour::text3);
    g.drawText (title, text.removeFromTop (titleLine), juce::Justification::centred, false);
    if (! isEditing())
    {
        g.setFont (font (size::fs4));
        g.setColour (colour::text1);
        g.drawText (value, text.removeFromTop (valueLine), juce::Justification::centred, false);
    }
}

void KnobTooltip::resized()
{
    if (editor != nullptr)
        editor->setBounds (juce::Rectangle<int> (fieldWidth, fieldHeight)
                               .withCentre ({ getWidth() / 2, paddingTop + titleLine + 1 + valueLine / 2 }));
}

void KnobTooltip::mouseDoubleClick (const juce::MouseEvent&)
{
    if (knob.isEnabled())
        startEditing();
}

void KnobTooltip::mouseExit (const juce::MouseEvent& e)
{
    if (! isEditing() && ! knob.isMouseButtonDown())
        knob.hideTooltipUnlessHovered (e.getScreenPosition());
}

void KnobTooltip::startEditing()
{
    if (isEditing())
        return;
    editor = std::make_unique<juce::TextEditor> ("type-in");
    editor->setTitle (title + " value");
    editor->setFont (font (size::fs3));
    editor->setJustification (juce::Justification::centred);
    editor->setIndents (6, 2);
    editor->setColour (juce::TextEditor::backgroundColourId, colour::fill2);
    editor->setColour (juce::TextEditor::outlineColourId, colour::line3);
    editor->setColour (juce::TextEditor::focusedOutlineColourId, colour::line3);
    editor->setColour (juce::TextEditor::textColourId, colour::text1);
    // The value as typed: without its unit.
    auto text = value;
    if (const auto suffix = knob.getTextValueSuffix(); suffix.isNotEmpty() && text.endsWith (suffix))
        text = text.dropLastCharacters (suffix.length());
    editor->setText (text, false);
    editor->onReturnKey = [this] { stopEditing (true); };
    editor->onEscapeKey = [this] { stopEditing (false); };
    editor->onFocusLost = [this] { stopEditing (false); };
    addAndMakeVisible (*editor);
    resized();
    repaint();
    if (editor->isShowing())
    {
        editor->grabKeyboardFocus();
        editor->selectAll();
    }
}

void KnobTooltip::stopEditing (bool commit)
{
    if (editor == nullptr)
        return;
    // Out of the way before anything else, so the focus it loses on the way out cancels nothing. It is
    // deleted later, as this may be its own key handler running.
    const auto text = editor->getText();
    std::shared_ptr<juce::TextEditor> closing (editor.release());
    closing->onFocusLost = nullptr;
    closing->onReturnKey = nullptr;
    closing->onEscapeKey = nullptr;
    removeChildComponent (closing.get());
    juce::MessageManager::callAsync ([closing] {});
    if (commit)
        knob.commitTypedText (text);
    refresh();
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<KnobTooltip> (this)] {
        if (safe != nullptr && ! safe->isEditing() && ! safe->isMouseOver (true))
            safe->knob.hideTooltipUnlessHovered (juce::Desktop::getMousePosition());
    });
}

} // namespace staple
