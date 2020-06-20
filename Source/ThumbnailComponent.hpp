#pragma once

#include <JuceHeader.h>

class ThumbnailComponent : public Component,
    public ChangeListener,
    public FileDragAndDropTarget,
    public ChangeBroadcaster,
    private Timer
{
public:
    ThumbnailComponent(AudioFormatManager& formatManager,
        AudioTransportSource& source)
        : transportSource(source),
        thumbnail(512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener(this);

        currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
        addAndMakeVisible(currentPositionMarker);
    }

    ~ThumbnailComponent() override
    {
        thumbnail.removeChangeListener(this);
    }

    void setURL(const URL& url)
    {
        InputSource* inputSource = new URLInputSource(url);
        if (inputSource != nullptr)
        {
            thumbnail.setSource(inputSource);

            Range<double> newRange(0.0, thumbnail.getTotalLength());
            setRange(newRange);

            startTimerHz(40);
        }
    }

    URL getLastDroppedFile() const noexcept { return lastFileDropped; }

    void setZoomFactor(double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax(0.001, thumbnail.getTotalLength() * (1.0 - jlimit(0.0, 0.99, amount)));
            auto timeAtCentre = xToTime(getWidth() / 2.0f);

            setRange({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
        }
    }

    void setRange(Range<double> newRange)
    {
        visibleRange = newRange;
        updateCursorPosition();
        repaint();
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(Colours::darkgrey).withAlpha(0.3f));
        g.setColour(Colours::lightblue);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto thumbArea = getLocalBounds();

            thumbnail.drawChannels(g, thumbArea.reduced(2),
                visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
        else
        {
            g.setFont(14.0f);
            g.drawFittedText("(No audio file selected)", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
    }

    void changeListenerCallback(ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override
    {
        return true;
    }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override
    {
        lastFileDropped = URL(File(files[0]));
        sendChangeMessage();
    }

    void mouseDown(const MouseEvent& e) override
    {
        mouseDrag(e);
    }

    void mouseDrag(const MouseEvent& e) override
    {
        transportSource.setPosition(jmax(0.0, xToTime((float)e.x)));
    }

    void mouseUp(const MouseEvent&) override
    {
    }

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        if (thumbnail.getTotalLength() > 0.0)
        {
            auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
            newStart = jlimit(0.0, jmax(0.0, thumbnail.getTotalLength() - (visibleRange.getLength())), newStart);

            setRange({ newStart, newStart + visibleRange.getLength() });

            repaint();
        }
    }


private:
    AudioTransportSource& transportSource;

    AudioThumbnailCache thumbnailCache{ 5 };
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;

    float timeToX(const double time) const
    {
        if (visibleRange.getLength() <= 0) { return 0; }

        return getWidth() * (float)((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime(const float x) const
    {
        return (x / getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    void timerCallback() override
    {
        updateCursorPosition();
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setRectangle(Rectangle<float>(timeToX(transportSource.getCurrentPosition()) - 0.75f, 0, 1.5f, (float)(getHeight())));
    }
};
