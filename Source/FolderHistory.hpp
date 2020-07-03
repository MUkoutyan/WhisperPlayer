#pragma once
#include <vector>
#include <JuceHeader.h>

class FolderHistory
{
	static constexpr std::int32_t InvalidIndex = -1;
public:
	FolderHistory() : currentIndex(InvalidIndex)
	{}

	void Add(String path)
	{
		if(currentIndex != InvalidIndex && currentIndex != (historyList.size() - 1)) {
			historyList.erase(historyList.begin() + currentIndex, historyList.end());
		}
		historyList.emplace_back(std::move(path));
	}

	String Next() noexcept
	{
		if(CanMoveNext() == false) { return ""; }
		currentIndex++;
		return CurrentIndicateDirectory();
	}

	String Prev() noexcept
	{
		if(CanMovePrev() == false) { return ""; }
		currentIndex--;
		return CurrentIndicateDirectory();
	}

	bool CanMoveNext() const noexcept
	{
		return currentIndex < (std::int32_t(historyList.size()) - 1);
	}

	bool CanMovePrev() const noexcept
	{
		return 0 < currentIndex;
	}

	String CurrentIndicateDirectory() const noexcept
	{
		if(currentIndex == InvalidIndex || historyList.size() <= currentIndex) {
			return "";
		}

		return historyList[currentIndex];
	}

private:
	std::int32_t currentIndex;
	std::vector<String> historyList;

};
