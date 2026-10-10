#pragma once

#include "../../KeyboardSlider.h"
#include "../Tokens.h"

#include <memory>
#include <optional>

namespace staple
{

class KnobTooltip;

// Staple's knob (HANDOFF.md §4 "Knobs", §5.3): a rotary Slider of any diameter, drawn as a lit face with
// a value arc in a caller-set colour and no track. A vertical drag covers the whole range over 200 px, or
// 800 px with Shift, as one gesture; a double-click resets it to the value its attachment set
// (setDoubleClickReturnValue: SliderAttachment sets the parameter's default) as one gesture. The arrow
// keys step it as any KeyboardSlider. While the pointer is over it, or it is dragged, a KnobTooltip shows
// its title and value text; a double-click on the tooltip types a value. Disabled, it dims to 35 % and
// ignores the mouse and keys.
//
// A knob can reserve a lane outside its face (the Gain knob's Dynamic Range ring): presses, drags and
// hovers there go to its RingHandler, which also paints the lane.
class Knob : public eq1::KeyboardSlider
{
public:
    explicit Knob (float diameter, const juce::String& name = {});
    ~Knob() override;

    float getDiameter() const { return diameter; }
    // The side of the square it needs: its face, the shadow under it and its ring lane.
    int getIdealSize() const;

    void setArcColour (juce::Colour colour);
    juce::Colour getArcColour() const { return arcColour; }
    // The arc starts from 12 o'clock.
    void setBipolar (bool bipolar);
    // The arc starts from this value, such as 0 dB on Output Gain's skewed range.
    void setArcOrigin (double value);

    // The outer lane, taken over by a RingHandler.
    struct RingHandler
    {
        virtual ~RingHandler() = default;
        // The lane is the annulus from inner to outer around centre, in the knob's coordinates.
        virtual void paintRing (juce::Graphics& g, Knob& knob, juce::Point<float> centre, float inner, float outer) = 0;
        virtual void ringMouseDown (Knob&, const juce::MouseEvent&) {}
        virtual void ringMouseDrag (Knob&, const juce::MouseEvent&) {}
        virtual void ringMouseUp (Knob&, const juce::MouseEvent&) {}
        virtual void ringDoubleClick (Knob&, const juce::MouseEvent&) {}
        virtual void ringHover (Knob&, bool over) { juce::ignoreUnused (over); }
    };
    // A lane width wide, centred offset outside the face's edge (the Gain knob's 12 px lane at r + 10).
    // nullptr removes it.
    void setRing (RingHandler* handler, float width = tokens::knob::ringLane, float offset = tokens::knob::ringOffset);
    bool isOnRing (juce::Point<float> position) const;

    // The face's centre and radius in the knob's coordinates.
    juce::Point<float> getFaceCentre() const;
    float getFaceRadius() const { return diameter / 2.0f; }

    // What the tooltip shows: the title (#48's, or the name) and the value text with its unit, as a
    // screen reader reads it (KeyboardSlider::describe), else its text.
    juce::String tooltipTitle() const;
    juce::String tooltipValue();
    bool isTooltipShown() const;
    KnobTooltip* getKnobTooltip() const { return tooltip.get(); }

    // Text typed into the tooltip: read by parseValue in the value suffix's unit, else by the parameter's
    // own text, clamped to the range and set as one gesture. False if the knob is disabled.
    bool commitTypedText (const juce::String& text);

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void valueChanged() override;
    void enablementChanged() override;
    void parentHierarchyChanged() override;

protected:
    // Shows the tooltip in the knob's overlay layer, where its type-in field opens.
    void showTooltip();

private:
    friend class KnobTooltip;
    void hideTooltipUnlessHovered (juce::Point<int> screenPosition);
    void hideTooltip();
    float arcRadius() const;
    float arcStartProportion();

    float diameter;
    juce::Colour arcColour = tokens::colour::text1;
    bool bipolar = false;
    std::optional<double> arcOrigin;

    RingHandler* ring = nullptr;
    float ringWidth = 0.0f, ringOffset = 0.0f;
    bool ringHovered = false;

    // A drag on the face: where it started and the proportion there, rebased when Shift changes.
    struct Drag
    {
        Drag (Knob& knob, float y, bool shift)
            : startY (y), startProportion (knob.valueToProportionOfLength (knob.getValue())), fine (shift), gesture (knob)
        {
        }
        float startY;
        double startProportion;
        bool fine;
        ScopedDragNotification gesture;
    };
    std::unique_ptr<Drag> drag;
    bool ringDragging = false;

    std::unique_ptr<KnobTooltip> tooltip;
};

} // namespace staple
