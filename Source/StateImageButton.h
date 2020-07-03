/*
  ==============================================================================

	StateImageButton.h
	Created: 4 Jul 2020 12:52:10am
	Author:  Koyo

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class StateImageButton : public ImageButton, public Button::Listener
{
public:
	StateImageButton(std::vector<Image> images, std::uint32_t initState = 0)
		: ImageButton()
		, images(std::move(images))
		, currentState(initState)
	{
		addListener(this);
		attachCurrentStateImage();
	}

	std::uint32_t getCurrentState() const noexcept { return currentState; }

protected:

	void attachCurrentStateImage()
	{
		jassert(currentState < images.size());
		auto& image = this->images[currentState];
		this->setImages(true, true, true, image, 1.0f, {}, image, 1.0f, {}, image, 1.0f, {});
	}

	void buttonClicked(Button* button)
	{
		currentState++;
		currentState %= images.size();

		const int beforeWidth = this->getWidth();
		const int beforeHeight = this->getHeight();
		attachCurrentStateImage();
		this->setSize(beforeWidth, beforeHeight);
		this->repaint();
	}

private:
	std::vector<Image> images;
	std::uint32_t currentState;
};
