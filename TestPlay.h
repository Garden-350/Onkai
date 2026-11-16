#pragma once

#include <string>
#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib,"shell32.lib")

struct TestPlaySettings
{
	bool enabled = false;	//テストプレイで起動されたか
	std::string chart;	//譜面の名前　charts/<chart>.json
	float start = 0.0f;	//何秒から始めるか
	bool autoMode = false;
};

namespace TestPlay
{
	inline TestPlaySettings& Get()
	{
		static TestPlaySettings s;
		return s;
	}
	inline void ParseCommandLine()
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (!argv) return;

		auto narrow = [](const wchar_t* w)
			{
				int n = WideCharToMultiByte(CP_ACP, 0, w, -1, nullptr, 0, nullptr, nullptr);
				std::string s(n > 0 ? n - 1 : 0, '\0');
				if (n > 1)WideCharToMultiByte(CP_ACP, 0, w, -1, s.data(), n, nullptr, nullptr);
				return s;
			};
		TestPlaySettings& s = Get();
		for (int i = 1; i < argc; i++)
		{
			std::wstring a = argv[i];
			if (a == L"--chart" && i + 1 < argc) { s.chart = narrow(argv[++i]); s.enabled = true; }
			else if (a == L"--start" && i + 1 < argc) { s.start = (float)_wtof(argv[++i]); }
			else if (a == L"--auto") { s.autoMode = true; }
		}
		LocalFree(argv);
	}
}