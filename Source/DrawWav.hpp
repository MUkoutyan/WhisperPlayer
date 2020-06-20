#pragma once

#include "ThumbnailComponent.hpp"
#include "WPAudioSource.hpp"
#include "FolderHistory.hpp"
#include "AudioSettings.h"

//==============================================================================

//==============================================================================
class DrawWav : public Component,
    public Slider::Listener,
    public FileDragAndDropTarget,
    private FileBrowserListener,
    private ChangeListener
{
    void ConvertReverseColor(Image& image)
    {
        auto bounds = image.getBounds();
        for (int y = 0; y < bounds.getHeight(); ++y)
        {
            for (int x = 0; x < bounds.getWidth(); ++x)
            {
                constexpr auto maxValue = std::numeric_limits<std::uint8_t>::max();
                auto color = image.getPixelAt(x, y);
                auto newColor = Colour().fromRGBA(maxValue - color.getRed(), maxValue - color.getGreen(), maxValue - color.getBlue(), color.getAlpha());
                image.setPixelAt(x, y, newColor);
            }
        }
    }

public:
    DrawWav() : volumeSlider(Slider::LinearHorizontal, Slider::NoTextBox)
        , currentAudioFileIndex(-1)
        , settingComponent(audioDeviceManager, 2, 2, 2, 2, false, false, true, true)
    {
#if JUCE_WINDOWS
        String typeFaceName = "Yu Gothic UI";
        this->getLookAndFeel().setDefaultSansSerifTypefaceName(typeFaceName);
#elif JUCE_MAC
        String typeFaceName = "Arial Unicode MS";
        this->getLookAndFeel().setDefaultSansSerifTypefaceName(typeFaceName);
#endif

        auto AddButton = [this](ImageButton& button, const char* image_path, const int image_size)
        {
            addAndMakeVisible(button);
            auto image = ImageCache::getFromMemory(image_path, image_size);
            ConvertReverseColor(image);
            button.setImages(true, true, true, image, 1.0f, {}, image, 1.0f, {}, image, 1.0f, {});
            button.setSize(32, 32);
        };

        AddButton(prevDirButton, BinaryData::arrowleft_png, BinaryData::arrowleft_pngSize);
        prevDirButton.onClick = [this]() {
            auto path = folderHistory.Prev();
            if (path == "") { return; }
            setDirectory(path);
        };

        AddButton(nextDirButton, BinaryData::arrowright_png, BinaryData::arrowright_pngSize);
        nextDirButton.onClick = [this]() {
            auto path = folderHistory.Next();
            if (path == "") { return; }
            setDirectory(path);
        };

        AddButton(moveUpFolder, BinaryData::folderuploadfill_png, BinaryData::folderuploadfill_pngSize);
        moveUpFolder.onClick = [this]() {
            auto currentFile = File(this->targetDirectory.getText());
            auto parentDir = currentFile.getParentDirectory();
            if (parentDir.exists() && parentDir.isDirectory()) {
                setDirectory(parentDir);
            }
        };

        addAndMakeVisible(targetDirectory);
        targetDirectory.onReturnKey = [this]() {
            if (File(this->targetDirectory.getText()).exists()) {
                setDirectory(File(this->targetDirectory.getText()));
            }
            else {
                targetDirectory.setText(directoryList.getDirectory().getFullPathName());
            }
        };

        AddButton(settingButton, BinaryData::settings_png, BinaryData::settings_pngSize);
        settingButton.onClick = [this]() 
        {
            DialogWindow::showModalDialog("Audio Settings", &this->settingComponent, nullptr, Colours::black, true);
        };


        addAndMakeVisible(fileTreeComp);

        setDirectory(File::getSpecialLocation(File::userDocumentsDirectory));

        fileTreeComp.setColour(FileTreeComponent::backgroundColourId, Colours::black.withAlpha(0.6f));
        fileTreeComp.addListener(this);

        thumbnail.reset(new ThumbnailComponent(formatManager, transportSource));
        addAndMakeVisible(thumbnail.get());
        thumbnail->addChangeListener(this);

        addAndMakeVisible(currentPlayPosition);
        currentPlayPosition.setColour(Label::textColourId, Colours::white);
        currentPlayPosition.setText("", dontSendNotification);

        AddButton(prevButton, BinaryData::rewindfill_png, BinaryData::rewindfill_pngSize);
        AddButton(playButton, BinaryData::playfill_png, BinaryData::playfill_pngSize);
        AddButton(pauseButton, BinaryData::pausefill_png, BinaryData::pausefill_pngSize);
        AddButton(stopButton, BinaryData::stopfill_png, BinaryData::stopfill_pngSize);
        AddButton(nextButton, BinaryData::speedfill_png, BinaryData::speedfill_pngSize);

        pauseButton.setVisible(false);

        playButton.onClick  = std::bind(&DrawWav::play, this);
        pauseButton.onClick = std::bind(&DrawWav::pause, this);
        stopButton.onClick  = std::bind(&DrawWav::stop, this);
        prevButton.onClick  = [this]()
        {
            if (1.5 < transportSource.getCurrentPosition()) {
                transportSource.setPosition(0.0);
                return;
            }
            updateAudioFileList();
            const int startFileIndex = jmax(0, currentAudioFileIndex);
            if (audioFileList.isEmpty()) { return; }
            while (true)
            {
                currentAudioFileIndex--;
                if (currentAudioFileIndex < 0) {
                    currentAudioFileIndex = audioFileList.size() - 1;
                }
                const auto& file = audioFileList[currentAudioFileIndex].getLocalFile();
                if (file.isDirectory() == false && audioFileFilter->isFileSuitable(file)) {
                    break;
                }

                if (currentAudioFileIndex == startFileIndex) { return; }

            }
            fileTreeComp.selectRow(currentAudioFileIndex);
        };
        nextButton.onClick = [this]()
        {
            updateAudioFileList();
            const int startFileIndex = jmax(0, currentAudioFileIndex);
            const auto size = audioFileList.size();
            if (size == 0) { return; }
            while (true)
            {
                currentAudioFileIndex++;
                if (size < currentAudioFileIndex) {
                    currentAudioFileIndex = 0;
                }
                const auto& file = audioFileList[currentAudioFileIndex].getLocalFile();
                if (file.isDirectory() == false && audioFileFilter->isFileSuitable(file)) {
                    break;
                }

                if (currentAudioFileIndex == startFileIndex) {
                    return;
                }
            }
            fileTreeComp.selectRow(currentAudioFileIndex);
        };

        addAndMakeVisible(volumeSlider);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(0.8);
        volumeSlider.addListener(this);

        addAndMakeVisible(isSwapLR);
        isSwapLR.setButtonText("Swap LR Channel");
        isSwapLR.setToggleState(false, dontSendNotification);
        isSwapLR.onClick = [this]() {
            this->transportSource.EnableSwapLR(isSwapLR.getToggleState());
        };

        // audio setup
        formatManager.registerBasicFormats();

        audioFileFilter.reset(new AudioFileFilter(formatManager.getWildcardForAllFormats()));
        directoryList.setFileFilter(this->audioFileFilter.get());

        thread.startThread(3);

        RuntimePermissions::request(RuntimePermissions::recordAudio,
            [this](bool granted)
            {
                int numInputChannels = granted ? 2 : 0;
                audioDeviceManager.initialise(numInputChannels, 2, nullptr, true, {}, nullptr);
            });

        audioDeviceManager.addAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(&transportSource);

        setOpaque(true);
        setSize(500, 420);
    }

    ~DrawWav() override
    {
        transportSource.setSource(nullptr);
        audioSourcePlayer.setSource(nullptr);

        audioDeviceManager.removeAudioCallback(&audioSourcePlayer);

        fileTreeComp.removeListener(this);

        thumbnail->removeChangeListener(this);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colours::black);

        Time currentTime(Time::getCurrentTime().getYear(), 0, 0, 0, 0, int(transportSource.getCurrentPosition()));
        currentPlayPosition.setText(currentTime.formatted("%H:%M:%S"), dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4);

        settingComponent.centreWithSize(r.getWidth(), r.getHeight());

        auto fileBrowserArea = r.removeFromTop(24);
        prevDirButton.setBounds(fileBrowserArea.removeFromLeft(32));
        nextDirButton.setBounds(fileBrowserArea.removeFromLeft(32));
        moveUpFolder.setBounds(fileBrowserArea.removeFromLeft(32));
        settingButton.setBounds(fileBrowserArea.removeFromRight(32));
        targetDirectory.setBounds(fileBrowserArea);

        r.removeFromTop(4);

        auto option = r.removeFromBottom(16);
        isSwapLR.setBounds(option);

        auto controls = r.removeFromBottom(32);
        constexpr int buttonSize = 32;
        auto positionTextArea = controls.removeFromLeft((controls.getWidth() / 2) - buttonSize * 2);
        currentPlayPosition.setBounds(positionTextArea.removeFromRight(64));

        prevButton.setBounds(controls.removeFromLeft(buttonSize));
        playButton.setBounds(controls.removeFromLeft(buttonSize));
        pauseButton.setBounds(playButton.getBounds());
        stopButton.setBounds(controls.removeFromLeft(buttonSize));
        nextButton.setBounds(controls.removeFromLeft(buttonSize));

        volumeSlider.setBounds(controls);

        thumbnail->setBounds(r.removeFromBottom(120));

        r.removeFromBottom(4);
        fileTreeComp.setBounds(r);
    }

private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    AudioDeviceManager audioDeviceManager;

    AudioFormatManager formatManager;
    TimeSliceThread thread{ "audio file preview" };

    FolderHistory folderHistory;

    TextEditor targetDirectory;

    ImageButton prevDirButton;
    ImageButton nextDirButton;
    ImageButton moveUpFolder;
    DirectoryContentsList directoryList{ nullptr, thread };
    FileListComponent fileTreeComp{ directoryList };
    ImageButton settingButton;
    AudioDeviceSelectorComponent settingComponent;

    Label currentPlayPosition;

    ImageButton prevButton;
    ImageButton playButton;
    ImageButton pauseButton;
    ImageButton stopButton;
    ImageButton nextButton;

    Slider volumeSlider;

    ToggleButton isSwapLR;

    int currentAudioFileIndex;
    Array<URL> audioFileList;
    URL currentAudioFile;
    AudioSourcePlayer audioSourcePlayer;
    WPAudioSource transportSource;
    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;

    std::unique_ptr<ThumbnailComponent> thumbnail;

    class AudioFileFilter : public FileFilter
    {
        StringArray supportExtentions;
    public:

        AudioFileFilter(StringArray list) : FileFilter("AudioFileFilter"), supportExtentions(std::move(list)) {}

        bool isFileSuitable(const File& file) const override
        {
            auto extention = file.getFileExtension();
            for (auto& obj : this->supportExtentions) {
                if (obj.contains(extention)) { return true; }
            }
            return false;
        }
        bool isDirectorySuitable(const File&) const override {
            return true;
        }
    };
    std::unique_ptr<AudioFileFilter> audioFileFilter;

    //==============================================================================
    void setDirectory(File dir)
    {
        auto path = dir.getFullPathName();
        if (folderHistory.CurrentIndicateDirectory() != path) {
            folderHistory.Add(path);
            folderHistory.Next();
        }
        directoryList.setDirectory(dir, true, true);
        targetDirectory.setText(directoryList.getDirectory().getFullPathName());
        audioFileList.clear();
    }

    void showAudioResource(URL resource)
    {
        if (loadURLIntoTransport(resource))
            currentAudioFile = std::move(resource);

        thumbnail->setURL(currentAudioFile);
    }

    bool loadURLIntoTransport(const URL& audioURL)
    {
        // unload the previous file source and delete it..
        bool isPlaying = transportSource.isPlaying();
        stop();
        transportSource.setSource(nullptr);
        currentAudioFileSource.reset();

        if (AudioFormatReader* reader = formatManager.createReaderFor(audioURL.createInputStream(false)))
        {
            currentAudioFileSource.reset(new AudioFormatReaderSource(reader, true));

            // ..and plug it into our transport source
            isSwapLR.setEnabled(reader->numChannels >= 2);
            transportSource.setSource(currentAudioFileSource.get(),
                32768,                   // tells it to buffer this many samples ahead
                &thread,                 // this is the background thread to use for reading-ahead
                reader->sampleRate);     // allows for sample rate correction

            if (isPlaying) { play(); }

            return true;
        }

        return false;
    }

    void play() {
        transportSource.start();
        playButton.setVisible(false);
        pauseButton.setVisible(true);
    }

    void pause() {
        transportSource.stop();
        pauseButton.setVisible(false);
        playButton.setVisible(true);
    }

    void stop() {
        transportSource.setPosition(0);
        transportSource.stop();
        pauseButton.setVisible(false);
        playButton.setVisible(true);
    }

    void updateAudioFileList()
    {
        if (audioFileList.isEmpty())
        {
            const auto numFiles = directoryList.getNumFiles();
            for (int i = 0; i < numFiles; ++i)
            {
                audioFileList.add(URL(directoryList.getFile(i)));
            }
        }
    }

    void selectionChanged() override
    {
        updateAudioFileList();

        auto url = URL(fileTreeComp.getSelectedFile());
        currentAudioFileIndex = -1;
        for (int i = 0; i < audioFileList.size(); ++i)
        {
            const auto& data = audioFileList[i];
            if (url.toString(true) == data.toString(true)) {
                currentAudioFileIndex = i;
                break;
            }
        }
        showAudioResource(url);
    }

    void sliderValueChanged(Slider* slider) override
    {
        transportSource.setGain(static_cast<float>(slider->getValue()));
    }

    void fileClicked(const File&, const MouseEvent&) override {
    }

    void fileDoubleClicked(const File& file) override
    {
        if (file.isDirectory()) {
            setDirectory(file);
        }
    }

    void browserRootChanged(const File&) override {}

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (source == thumbnail.get())
            showAudioResource(URL(thumbnail->getLastDroppedFile()));
    }

    bool isInterestedInFileDrag(const StringArray&) override {
        return true;
    }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override
    {
        auto file = File(files[0]);
        if (file.isDirectory()) {
            setDirectory(file);
        }
        else {
            setDirectory(file.getParentDirectory());
            showAudioResource(URL(file));
        }

    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawWav)
};
