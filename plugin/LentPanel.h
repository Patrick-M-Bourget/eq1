#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace eq1
{

// A panel shown in a call-out while it is open (the footer's, the Band panel's dynamics). The call-out
// hides itself as it closes, and is deleted later: the panel goes back to its home component, hidden,
// as soon as it hides.
class LentPanel final : public juce::Component, private juce::ComponentListener
{
public:
    LentPanel (juce::Component& p, juce::Component& h) : panel (&p), home (&h)
    {
        setSize (p.getWidth(), p.getHeight());
        p.setTopLeftPosition (0, 0);
        addAndMakeVisible (p);
    }

    ~LentPanel() override
    {
        if (callOut != nullptr)
            callOut->removeComponentListener (this);
        giveBack();
    }

    // Once in the call-out, follows it.
    void parentHierarchyChanged() override
    {
        if (callOut == nullptr && getParentComponent() != nullptr)
        {
            callOut = getParentComponent();
            callOut->addComponentListener (this);
        }
    }

private:
    void componentVisibilityChanged (juce::Component& component) override
    {
        if (! component.isVisible())
            giveBack();
    }

    void giveBack()
    {
        if (panel != nullptr && home != nullptr && panel->getParentComponent() == this)
        {
            panel->setVisible (false);
            home->addChildComponent (*panel);
        }
    }

    juce::Component::SafePointer<juce::Component> panel, home, callOut;
};

} // namespace eq1
