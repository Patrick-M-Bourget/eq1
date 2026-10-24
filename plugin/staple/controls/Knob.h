#pragma once

#include "../../KeyboardSlider.h"
#include "../Tokens.h"

#include <memory>
#include <optional>

namespace staple
{

class KnobTooltip;

// Where a vertical drag of pixels from a slider's value from lands, as on a Knob: the whole range over
// knob::dragPixels, or knob::fineDragPixels when fine (Shift), within the range.
double draggedAlongRange (juce::Slider& slider, double from, float pixels, bool fine);

// Staple's knob (HANDOFF.md §4 "Knobs", §5.3): a rotary Slider of any diameter, drawn as a lit face with
// a value arc in a caller-set colour and no track. A vertical drag covers the whole range over 200 px, or
// 800 px with Shift, as one gesture; a double-click resets it to the value its attachment set
// (setDoubleClickReturnValue: SliderAttachment sets the parameter's default), and a drag on its second
// press carries on from there, as one gesture (KeyboardSlider::startMouseDrag). The arrow
// keys step it as any KeyboardSlider. While the pointer is over it, or it is dragged, a KnobTooltip shows
// its title and value text; a double-click on the tooltip types a value. Showing it hides any other knob's on
// the same overlay layer, unless a value is being typed into that one. Disabled, it dims to 35 % and
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
        virtual void ringHover (Knob&, bool over) { juce::ignoreUnused (over); }
        // What the tooltip shows while the lane is hovered or dragged ("Band 4 Dynamic Range", "+6.0 dB").
        virtual juce::String ringTitle() { return {}; }
        virtual juce::String ringValue() { return {}; }
    };
    // A lane width wide, centred offset outside the face's edge (the Gain knob's 12 px lane at r + 10).
    // nullptr removes it.
    void setRing (RingHandler* handler, float width = tokens::knob::ringLane, float offset = tokens::knob::ringOffset);
    bool isOnRing (juce::Point<float> position) const;
    // The ring's value changed: repaints it, and the tooltip while it shows the ring.
    void ringChanged();

    // The face's centre and radius in the knob's coordinates.
    juce::Point<float> getFaceCentre() const;
    float getFaceRadius() const { return diameter / 2.0f; }

    // What the tooltip shows: the title, else the name, and the value text with its unit, as a screen
    // reader reads it (KeyboardSlider::describe), else its text; the ring's while its lane is hovered or
    // dragged, and after, until the pointer is next on the face or ring (on the way to the Readout, its content stays).
    juce::String tooltipTitle() const;
    bool isTooltipOnRing() const { return ring != nullptr && (readoutOnRing || ringDragging); }
    juce::String tooltipValue();
    bool isTooltipShown() const;
    KnobTooltip* getKnobTooltip() const { return tooltip.get(); }
    // The label under the knob, which its Knob Readout covers from the label's top; without one the
    // Knob Readout sits just under the face.
    void setLabel (juce::Component* newLabel) { label = newLabel; }
    juce::Component* getLabel() const { return label.getComponent(); }

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override {}
    void valueChanged() override;
    void enablementChanged() override;
    void parentHierarchyChanged() override;

protected:
    // Shows the tooltip in the knob's overlay layer, where its type-in field opens.
    void showTooltip();

private:
    double valueDraggedBy (double from, float pixels, bool fine) override;
    friend class KnobTooltip;
    // Hides the Readout unless the pointer at screenPosition is over the face (or ring) of the knob, enabled, or on its
    // way from there to the Readout, or on the Readout.
    void hideTooltipUnlessHovered (juce::Point<int> screenPosition);
    // The Readout's watchdog's check, with the pointer as the OS has it: hides the Readout as the pointer leaving would,
    // unless the knob is being dragged.
    void watchTooltip (juce::Point<float> pointer);
    void hideTooltip();
    // Whether e is on the ring as JUCE found the knob under it: at the whole pixel nearest its position (hitTest), so
    // the face and ring it reaches agree with the hit test at fractional positions (the UI Scale's).
    bool isOnRingAsHit (const juce::MouseEvent& e) const;
    // The face's square, in the knob's coordinates.
    juce::Rectangle<float> faceArea() const;
    // The radius round the face's centre that the hit area reaches: the face's, or the ring's outer edge.
    float hitRadius() const;
    // Whether the pointer at screenPosition crosses between the hit area and the Readout showing, in the Readout's
    // window: in the convex hull of the hit area's circle and the Readout.
    bool isOnWayToReadout (juce::Point<float> screenPosition);
    float arcRadius() const;
    float arcStartProportion();

    float diameter;
    juce::Colour arcColour = tokens::colour::text1;
    bool bipolar = false;
    std::optional<double> arcOrigin;

    RingHandler* ring = nullptr;
    float ringWidth = 0.0f, ringOffset = 0.0f;
    bool ringHovered = false;
    // Whether the pointer was on the ring when last on the face or ring: the Readout shows the ring's content.
    bool readoutOnRing = false;

    bool ringDragging = false;

    std::unique_ptr<KnobTooltip> tooltip;
    juce::Component::SafePointer<juce::Component> label;
};

} // namespace staple
