#pragma once
// ============================================================
// FileBrowser : 「ファイル」の窓。譜面（charts/*.json）と曲（asset/bgm/*）を一覧から選ぶ
//   ・譜面タブ … クリック → 「開く？」の確認 → 開く（保存してから開くこともできる）
//   ・曲タブ   … クリックで、その曲を今の譜面の曲にする（すぐ読み込む。譜面に残すには保存）
//   フォルダの中は数秒ごとに見直すので、ファイルを足したり消したりしてもそのまま出てくる
// ============================================================
#include <functional>
#include <string>

namespace FileBrowser
{
	struct Actions
	{
		std::function<void(const std::string&)> openChart;		// 譜面を開く（名前は .json なし）
		std::function<bool()> saveChart;						// 今の譜面を保存する（成功したら true）
		std::function<void(const std::string&)> setAudio;		// 曲を変える（asset/bgm/ の中のファイル名）
	};

	//   currentChart : 今の譜面の名前（.json なし）  currentAudio : 今の曲のファイル名
	void DrawPanel(const std::string& currentChart, const std::string& currentAudio, const Actions& actions);
}
