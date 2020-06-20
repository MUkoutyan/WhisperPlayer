/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
    : demo()
{
    addAndMakeVisible(demo);

    setBounds(demo.getBounds());
}

MainComponent::~MainComponent()
{
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    demo.paint(g);
}

void MainComponent::resized()
{
    demo.resized();
    // This is called when the MainComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
