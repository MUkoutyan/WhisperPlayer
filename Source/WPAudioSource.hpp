#pragma once
#include <JuceHeader.h>

class WPAudioSource : public AudioTransportSource
{
public:
	WPAudioSource() 
		: AudioTransportSource()
		, isSwapLR(false)
	{
	}
	~WPAudioSource(){}

	void getNextAudioBlock(const AudioSourceChannelInfo& info) override
	{
		AudioTransportSource::getNextAudioBlock(info);

		if (isSwapLR && 2 <= info.buffer->getNumChannels()) {
			auto buffer = info.buffer->getArrayOfWritePointers();
			for (int s = 0; s < info.buffer->getNumSamples(); ++s)
			{
				auto temp = *(buffer[0] + s);
				*(buffer[0] + s) = *(buffer[1]+s);
				*(buffer[1] + s) = temp;
			}
		}
	}

	void EnableSwapLR(bool enable) noexcept { isSwapLR = enable; }

private:
	bool isSwapLR;

};

