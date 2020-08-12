#pragma once

#include "ThumbnailComponent.hpp"
#include "WPAudioSource.hpp"
#include "StateImageButton.h"
#include "FolderHistory.hpp"
#include "AudioSettings.h"

#include <stack>

//==============================================================================

//==============================================================================
class DrawUI : public Component,
			   public Slider::Listener,
			   public FileDragAndDropTarget,
			   public Timer,
			   private FileBrowserListener,
			   private ChangeListener,
			   private KeyListener
{
	static constexpr int buttonSize = 32;
	enum class LoopState : std::uint32_t
	{
		None = 0,
		One,
		All
	};

	void ConvertReverseColor(Image& image)
	{
		auto bounds = image.getBounds();
		for(int y = 0; y < bounds.getHeight(); ++y)
		{
			for(int x = 0; x < bounds.getWidth(); ++x)
			{
				constexpr auto maxValue = std::numeric_limits<std::uint8_t>::max();
				auto color = image.getPixelAt(x, y);
				auto newColor = Colour().fromRGBA(maxValue - color.getRed(), maxValue - color.getGreen(), maxValue - color.getBlue(), color.getAlpha());
				image.setPixelAt(x, y, newColor);
			}
		}
	}

	Image GetImageFromMemory(const char* image_path, const int image_size)
	{
		auto image = ImageCache::getFromMemory(image_path, image_size);
		ConvertReverseColor(image);
		return image;
	}

	void SetButtonImages(ImageButton& button, const char* image_path, const int image_size)
	{
		auto image = GetImageFromMemory(image_path, image_size);
		button.setImages(true, true, true, image, 1.0f, {}, image, 1.0f, {}, image, 1.0f, {});
		button.setSize(buttonSize, buttonSize);
	}

	void AddButton(ImageButton& button, const char* image_path, const int image_size)
	{
		addAndMakeVisible(button);
		SetButtonImages(button, image_path, image_size);
	}

public:
	DrawUI()
		: properties()
		, audioSourcePlayer()    //createAudioDeviceManager内で使用するため、audioDeviceManagerよりも先に初期化する
		, audioDeviceManager(createAudioDeviceManager())
		, numHistorySize(16)
		, directoryHistory()
		, volumeSlider(Slider::LinearHorizontal, Slider::NoTextBox)
		, currentAudioFileIndex(-1)
		, settingComponent(*audioDeviceManager, 2, 2, 2, 2, false, false, true, true)
		, loopButton(
			{
			  GetImageFromMemory(BinaryData::restart_none_png, BinaryData::restart_none_pngSize),
			  GetImageFromMemory(BinaryData::restart_one_png,  BinaryData::restart_one_pngSize),
			  GetImageFromMemory(BinaryData::restart_all_png,  BinaryData::restart_all_pngSize)
			},
			static_cast<std::uint32_t>(LoopState::None)
		)
		, isSuspendSaveProperty(true)	//プロパティのロードが終わるまで保存を禁止
	{
		setPropertiesOption(); 

#if JUCE_WINDOWS
		String typeFaceName = "Yu Gothic UI";
		this->getLookAndFeel().setDefaultSansSerifTypefaceName(typeFaceName);
#elif JUCE_MAC
		String typeFaceName = "Arial Unicode MS";
		this->getLookAndFeel().setDefaultSansSerifTypefaceName(typeFaceName);
#endif
		setupUIComponents();

		addKeyListener(this);

		//audioFileFilterはフォーマットを登録してから作成する。
		formatManager.registerBasicFormats();
		audioFileFilter.reset(new AudioFileFilter(formatManager.getWildcardForAllFormats()));
		directoryList.setFileFilter(this->audioFileFilter.get());

		thread.startThread(3);

		audioSourcePlayer.setSource(&transportSource);

		loadProperties();

		startTimerHz(20); //再生状態監視用のタイマー

		setOpaque(true);
		setSize(500, 420);
	}

	void setupUIComponents()
	{
		AddButton(prevDirButton, BinaryData::arrowleft_png, BinaryData::arrowleft_pngSize);
		prevDirButton.onClick = [this]() {
			auto path = folderHistory.Prev();
			if(path == "") { return; }
			setDirectory(path);
		};

		AddButton(nextDirButton, BinaryData::arrowright_png, BinaryData::arrowright_pngSize);
		nextDirButton.onClick = [this]() {
			auto path = folderHistory.Next();
			if(path == "") { return; }
			setDirectory(path);
		};

		AddButton(moveUpFolder, BinaryData::folderuploadfill_png, BinaryData::folderuploadfill_pngSize);
		moveUpFolder.onClick = std::bind(&DrawUI::moveParentDirectory, this);

		addAndMakeVisible(targetDirectory);
		targetDirectory.onReturnKey = [this]() {
			if(File(this->targetDirectory.getText()).exists()) {
				setDirectory(File(this->targetDirectory.getText()));
			}
			else {
				targetDirectory.setText(directoryList.getDirectory().getFullPathName());
			}
		};

		AddButton(showHistoryButton, BinaryData::arrowdownsfill_png, BinaryData::arrowdownsfill_pngSize);
		showHistoryButton.onClick = [this]()
		{
			directoryHistoryMenu.clear();
			for(const auto& str : directoryHistory){
				directoryHistoryMenu.addItem(str, [this, &str](){ setDirectory(str); });
			}

			PopupMenu::Options options;
			options.withTargetComponent(targetDirectory);
			directoryHistoryMenu.showMenuAsync(options);
		};

		AddButton(settingButton, BinaryData::settings_png, BinaryData::settings_pngSize);
		settingButton.onClick = [this]()
		{
			DialogWindow::showModalDialog("Audio Settings", &this->settingComponent, nullptr, Colours::black, true);
		};

		//fileTreeComp.setColour(FileTreeComponent::backgroundColourId, Colours::black);
		fileTreeComp.addListener(this);
		addAndMakeVisible(fileTreeComp);

		thumbnail.reset(new ThumbnailComponent(formatManager, transportSource));
		addAndMakeVisible(thumbnail.get());
		thumbnail->addChangeListener(this);

		addAndMakeVisible(currentPlayTimeText);
		currentPlayTimeText.setColour(Label::textColourId, Colours::white);
		currentPlayTimeText.setText("", dontSendNotification);

		AddButton(prevButton,  BinaryData::rewindfill_png, BinaryData::rewindfill_pngSize);
		AddButton(playButton,  BinaryData::playfill_png,   BinaryData::playfill_pngSize);
		AddButton(pauseButton, BinaryData::pausefill_png,  BinaryData::pausefill_pngSize);
		AddButton(stopButton,  BinaryData::stopfill_png,   BinaryData::stopfill_pngSize);
		AddButton(nextButton,  BinaryData::speedfill_png,  BinaryData::speedfill_pngSize);

		addAndMakeVisible(loopButton);
		loopButton.setSize(buttonSize, buttonSize);

		pauseButton.setVisible(false);

		playButton.onClick  = std::bind(&DrawUI::play, this);
		pauseButton.onClick = std::bind(&DrawUI::pause, this);
		stopButton.onClick  = std::bind(&DrawUI::stop, this);
		prevButton.onClick  = std::bind(&DrawUI::MoveToPrevAudio, this);
		nextButton.onClick  = std::bind(&DrawUI::MoveToNextAudio, this);

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
	}

	~DrawUI() override
	{
		stop();
		saveProperties();

		transportSource.setSource(nullptr);
		audioSourcePlayer.setSource(nullptr);

		audioDeviceManager->removeAudioCallback(&audioSourcePlayer);

		fileTreeComp.removeListener(this);

		thumbnail->removeChangeListener(this);
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colour(0xff003b57));

		Time currentTime(Time::getCurrentTime().getYear(), 0, 0, 0, 0, int(transportSource.getCurrentPosition()));
		currentPlayTimeText.setText(currentTime.formatted("%H:%M:%S"), dontSendNotification);
	}

	void resized() override
	{
		auto r = getLocalBounds().reduced(4);

		settingComponent.centreWithSize(r.getWidth() - buttonSize, r.getHeight());

		auto fileBrowserArea = r.removeFromTop(24);
		prevDirButton.setBounds(fileBrowserArea.removeFromLeft(buttonSize));
		nextDirButton.setBounds(fileBrowserArea.removeFromLeft(buttonSize));
		moveUpFolder.setBounds(fileBrowserArea.removeFromLeft(buttonSize));
		settingButton.setBounds(fileBrowserArea.removeFromRight(buttonSize));
		showHistoryButton.setBounds(fileBrowserArea.removeFromRight(buttonSize));
		targetDirectory.setBounds(fileBrowserArea);

		r.removeFromTop(4);

		auto controls = r.removeFromBottom(buttonSize + 4);

		auto positionTextArea = controls.removeFromLeft((controls.getWidth() / 2) - buttonSize * 2);
		currentPlayTimeText.setBounds(positionTextArea.removeFromRight(64));

		isSwapLR.setBounds(positionTextArea);

		prevButton.setBounds(controls.removeFromLeft(buttonSize));
		playButton.setBounds(controls.removeFromLeft(buttonSize));
		pauseButton.setBounds(playButton.getBounds());
		stopButton.setBounds(controls.removeFromLeft(buttonSize));
		nextButton.setBounds(controls.removeFromLeft(buttonSize));
		loopButton.setBounds(controls.removeFromLeft(buttonSize));

		controls.setWidth(jmin(controls.getWidth(), 200));
		volumeSlider.setBounds(controls);

		thumbnail->setBounds(r.removeFromBottom(120));

		r.removeFromBottom(4);
		fileTreeComp.setBounds(r);
	}

private:
	ApplicationProperties properties;
	PropertiesFile* propertiesFile;
	AudioSourcePlayer audioSourcePlayer;
	std::unique_ptr<AudioDeviceManager> audioDeviceManager;
	std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;
	AudioDeviceSelectorComponent settingComponent;

	AudioFormatManager formatManager;
	TimeSliceThread thread{ "audio file preview" };

	FolderHistory folderHistory;
	TextEditor targetDirectory;
	PopupMenu  directoryHistoryMenu;
	ImageButton showHistoryButton;
	size_t numHistorySize;
	StringArray directoryHistory;

	ImageButton prevDirButton;
	ImageButton nextDirButton;
	ImageButton moveUpFolder;
	DirectoryContentsList directoryList{ nullptr, thread };
	FileListComponent fileTreeComp{ directoryList };
	int currentAudioFileIndex;
	Array<URL> audioFileList;
	URL currentAudioFile;

	ImageButton settingButton;

	WPAudioSource transportSource;

	ToggleButton isSwapLR;
	Label currentPlayTimeText;
	ImageButton prevButton;
	ImageButton playButton;
	ImageButton pauseButton;
	ImageButton stopButton;
	ImageButton nextButton;
	StateImageButton loopButton;
	Slider volumeSlider;


	std::unique_ptr<ThumbnailComponent> thumbnail;


	class AudioFileFilter : public FileFilter
	{
		StringArray supportExtentions;
	public:

		AudioFileFilter(StringArray list) : FileFilter("AudioFileFilter"), supportExtentions(std::move(list)) {}

		bool isFileSuitable(const File& file) const override
		{
			auto extention = file.getFileExtension();
			for(auto& obj : this->supportExtentions) {
				if(obj.contains(extention)) { return true; }
			}
			return false;
		}
		bool isDirectorySuitable(const File&) const override
		{
			return true;
		}
	};
	std::unique_ptr<AudioFileFilter> audioFileFilter;

	//=====================================================================================
	std::unique_ptr<AudioDeviceManager> createAudioDeviceManager()
	{
		auto deviceManager = std::make_unique<AudioDeviceManager>();
		//Inputは使用しないので0
		deviceManager->initialise(0, 2, nullptr, true);
		deviceManager->addAudioCallback(&audioSourcePlayer);
		return deviceManager;
	}

	void setPropertiesOption()
	{
		PropertiesFile::Options options;
		options.applicationName = ProjectInfo::projectName;
		options.filenameSuffix  = ".settings";
		options.folderName      = ProjectInfo::companyName;
		properties.setStorageParameters(options);

		propertiesFile = properties.getUserSettings();
	}

	void saveProperties()
	{
		if(isSuspendSaveProperty){ return; }
		propertiesFile->setValue("CurrentDirectory", targetDirectory.getText());
		propertiesFile->setValue("NumHistory",		 String(numHistorySize));

		String historyStr;
		directoryHistory.removeEmptyStrings();
		directoryHistory.removeDuplicates(false);
		for(const auto& str : directoryHistory){
			historyStr += str + ";";
		}
		propertiesFile->setValue("DirectoryHistory", historyStr);

		propertiesFile->setValue("LoopState",		 String(this->loopButton.getCurrentState()));
		propertiesFile->save();
	}

	void loadProperties()
	{
		auto dir = propertiesFile->getValue("CurrentDirectory", File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName());
		setDirectory(dir);

		auto numHistory = propertiesFile->getValue("NumHistory", "16");
		numHistorySize = numHistory.getIntValue();

		auto str = propertiesFile->getValue("DirectoryHistory", "");
		directoryHistory.addTokens(str, ";", "");
		directoryHistory.removeEmptyStrings();
		directoryHistory.removeDuplicates(false);
		
		auto loopStateStr = propertiesFile->getValue("LoopState", "0");
		this->loopButton.setCurrentState(loopStateStr.getIntValue());

		resumeSaveProperty();
	}

	bool isSuspendSaveProperty;
	void suspendSaveProperty() { isSuspendSaveProperty = true; }
	void resumeSaveProperty()  { isSuspendSaveProperty = false; }

	//==============================================================================
	void moveParentDirectory()
	{
		auto currentFile = File(this->targetDirectory.getText());
		auto parentDir = currentFile.getParentDirectory();
		if(parentDir.exists() && parentDir.isDirectory()) {
			setDirectory(parentDir);
		}
	}

	void setDirectory(const File& dir)
	{
		auto path = dir.getFullPathName();
		if(folderHistory.CurrentIndicateDirectory() != path) {
			folderHistory.Add(path);
			folderHistory.Next();
		}
		directoryList.setDirectory(dir, true, true);
		targetDirectory.setText(directoryList.getDirectory().getFullPathName());
		directoryHistory.add(path);
		if(numHistorySize <= directoryHistory.size()){
			directoryHistory.remove(0);
		}

		saveProperties();
		audioFileList.clear();
	}

	void showAudioResource(URL resource)
	{
		if(loadURLIntoTransport(resource)){
			updateAudioFileList();
			currentAudioFile = std::move(resource);
			thumbnail->setURL(currentAudioFile);
		}
	}

	bool loadURLIntoTransport(const URL& audioURL)
	{
		AudioFormatReader* reader = formatManager.createReaderFor(audioURL.createInputStream(false));
		if(reader == nullptr){ return false; }

		const bool isPlaying = transportSource.isPlaying();
		stop();
		transportSource.setSource(nullptr);	//必須。入れないとファイル切替時にクラッシュする。
		currentAudioFileSource.reset(new AudioFormatReaderSource(reader, true));

		isSwapLR.setEnabled(reader->numChannels >= 2);
		transportSource.setSource(currentAudioFileSource.get(),
								  32768,                   // tells it to buffer this many samples ahead
								  &thread,                 // this is the background thread to use for reading-ahead
								  reader->sampleRate);     // allows for sample rate correction

		if(isPlaying) { play(); }

		return true;
	}

	void MoveToNextAudio()
	{
		//updateAudioFileList();
		const int startFileIndex = jmax(0, currentAudioFileIndex);
		const auto size = audioFileList.size();
		if(size == 0) { return; }
		while(true)
		{
			currentAudioFileIndex++;
			if(size <= currentAudioFileIndex) {
				currentAudioFileIndex = 0;
			}
			const auto& file = audioFileList[currentAudioFileIndex].getLocalFile();
			if(file.isDirectory() == false && audioFileFilter->isFileSuitable(file)) {
				break;
			}

			if(currentAudioFileIndex == startFileIndex) {
				return;
			}
		}
		fileTreeComp.selectRow(currentAudioFileIndex);
	}

	void MoveToPrevAudio()
	{
		if(1.5 < transportSource.getCurrentPosition()) {
			transportSource.setPosition(0.0);
			return;
		}
		//updateAudioFileList();
		const int startFileIndex = jmax(0, currentAudioFileIndex);
		if(audioFileList.isEmpty()) { return; }
		while(true)
		{
			currentAudioFileIndex--;
			if(currentAudioFileIndex < 0) {
				currentAudioFileIndex = audioFileList.size() - 1;
			}
			const auto& file = audioFileList[currentAudioFileIndex].getLocalFile();
			if(file.isDirectory() == false && audioFileFilter->isFileSuitable(file)) {
				break;
			}

			if(currentAudioFileIndex == startFileIndex) { return; }

		}
		fileTreeComp.selectRow(currentAudioFileIndex);
	}

	void selectionChanged() override
	{
		auto url = URL(fileTreeComp.getSelectedFile());
		currentAudioFileIndex = -1;
		const auto size = audioFileList.size();
		for(int i = 0; i < size; ++i)
		{
			const auto& data = audioFileList[i];
			if(url.toString(true) == data.toString(true)) {
				currentAudioFileIndex = i;
				break;
			}
		}
		showAudioResource(url);
	}

	void updateAudioFileList()
	{
		//ディレクトリの変更後にaudioFileListがクリアされる。
		if(audioFileList.isEmpty())
		{
			const auto numFiles = directoryList.getNumFiles();
			for(int i = 0; i < numFiles; ++i)
			{
				audioFileList.add(URL(directoryList.getFile(i)));
			}
		}
	}

	//=====================================================================================
	void fileClicked(const File&, const MouseEvent&) override
	{}

	void fileDoubleClicked(const File& file) override
	{
		if(file.isDirectory()) {
			setDirectory(file);
		}
	}

	void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override
	{
		auto file = File(files[0]);
		if(file.isDirectory()) {
			setDirectory(file);
		}
		else {
			setDirectory(file.getParentDirectory());
			showAudioResource(URL(file));
		}
	}

	//=====================================================================================
	void play()
	{
		transportSource.start();
		playButton.setVisible(false);
		pauseButton.setVisible(true);
	}

	void pause()
	{
		transportSource.stop();
		pauseButton.setVisible(false);
		playButton.setVisible(true);
	}

	void stop()
	{
		transportSource.setPosition(0);
		transportSource.stop();
		pauseButton.setVisible(false);
		playButton.setVisible(true);
	}

	void sliderValueChanged(Slider* slider) override
	{
		transportSource.setGain(static_cast<float>(slider->getValue()));
	}

	void browserRootChanged(const File&) override {}

	void changeListenerCallback(ChangeBroadcaster* source) override
	{
		if(source == thumbnail.get()){
			showAudioResource(URL(thumbnail->getLastDroppedFile()));
		}
	}

	void timerCallback() override
	{
		//中断ボタンが表示されている==再生中
		if(this->pauseButton.isVisible() == false){ return; }
		if(transportSource.isPlaying()){ return; }

		//再生位置が終端に行ったことによる終了を想定
		auto state = static_cast<LoopState>(this->loopButton.getCurrentState());
		switch(state)
		{
			case LoopState::None:
				stop();
				break;
			case LoopState::One:
				transportSource.setPosition(0);
				play();
				break;
			case LoopState::All:
				MoveToNextAudio();
				play();
				break;
		}
	}

	bool isInterestedInFileDrag(const StringArray&) override
	{
		return true;
	}

	bool keyPressed(const KeyPress& key, Component* originatingComponent) override
	{
		const auto keyCode    = key.getKeyCode();
		const auto modifyCode = key.getModifiers();
		if(keyCode == KeyPress::spaceKey)
		{
			if(transportSource.isPlaying()){
				pause();
			}
			else{
				play();
			}
			return true;
		}
		else if(keyCode == KeyPress::leftKey && modifyCode == ModifierKeys::ctrlModifier)
		{
			moveParentDirectory();
		}

		return false;
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawUI)
};
