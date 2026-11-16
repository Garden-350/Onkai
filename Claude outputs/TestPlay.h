#pragma once
// ============================================================
// TestPlay : エディターの「テストプレイ」から起動されたときの設定
//   Sounder.exe --chart 譜面の名前 --start 開始の秒 [--auto]
//   --chart があれば、タイトル・セレクトを飛ばしてステージへ直行し、--start 秒から始める
// ============================================================
#include <string>
#include <windows.h>
#include <shellapi.h>					// CommandLineToArgvW
#pragma comment(lib, "shell32.lib")

struct TestPlaySettings
{
	bool enabled = false;		// テストプレイで起動されたか
	std::string chart;			// 譜面の名前（charts/<chart>.json）
	float start = 0.0f;			// 何秒から始めるか（曲の中の秒）
	bool autoMode = false;		// オートで流すか
};

namespace TestPlay
{
	// 設定は 1 つだけ（関数の中の static：最初に呼ばれたときに作られる）
	inline TestPlaySettings& Get()
	{
		static TestPlaySettings s;
		return s;
	}

	// 起動したときの引数を読む（Game::Init で 1 回だけ呼ぶ）
	inline void ParseCommandLine()
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (!argv) return;

		// ワイド文字 → いつもの文字列（ファイル名に使うので ANSI＝Shift-JIS に直す）
		auto narrow = [](const wchar_t* w)
			{
				int n = WideCharToMultiByte(CP_ACP, 0, w, -1, nullptr, 0, nullptr, nullptr);
				std::string s(n > 0 ? n - 1 : 0, '\0');
				if (n > 1) WideCharToMultiByte(CP_ACP, 0, w, -1, s.data(), n, nullptr, nullptr);
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
