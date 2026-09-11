#pragma once
#include <string>
struct ResultData
{
	int perfect = 0;
	int great = 0;
	int good = 0;
	int miss = 0;
	int maxCombo = 0;
	int totalNotes = 0;
	bool isFullCombo = false;
	std::string chartName;
};