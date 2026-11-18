#pragma once
// ============================================================
// ChartGimmicks : 譜面の演出（ギミック）をエディターで置く
//   いまは「速さ」だけ：逆走・低速・止める・速くする（全部 type "speed"、速さの数字が違うだけ）
//
//   「演出ノーツ」として置く（11/17 イースターエッグ案）：
//     譜面の右の「演出の列」に、ノーツの形（頭）＋ロングのような尾で出る
//     頭が判定ラインを通った所で演出が始まり、尾の端で終わる。本番では見えない
//   置き方：「演出」の窓で種類を選ぶ → 列をクリック（1 小節ぶん置かれる）。Esc・何もない所の右クリックでやめる
//   直し方：頭をドラッグで丸ごと動かす、尾の端をドラッグで長さ、右クリックで消す
//   演出を選んでいる時の F5 は、その演出の小節から（いつもの「何小節前から」も効く）
//
//   保存："gimmicks":[ { "type":"speed", "start":秒, "end":秒, "easeIn":拍, "easeOut":拍, "speed":-1 }, ... ]
//   ゲームの Gimmick.h / ChartLoader.cpp と同じ形にする
// ============================================================
#include <filesystem>
#include <functional>
#include <ostream>
#include <string>
#include <vector>
#include "json.hpp"
#include "ChartExtras.h"	// EditorTimeInfo

struct ImDrawList;

struct EditorGimmick
{
	std::string type = "speed";
	float start = 0.0f;		// 始まり（秒）
	float end = 0.0f;		// 終わり（秒）
	float easeIn = 0.5f;	// 入り（拍）
	float easeOut = 0.5f;	// 抜け（拍）
	float speed = 1.0f;		// 1=普通 0.5=半分 0=止まる -1=同じ速さで逆
};

namespace ChartGimmicks
{
	// ---- 保存・読み込み ----
	void OnLoad(const nlohmann::json& j);	// LoadChart で JSON を読んだ直後に呼ぶ
	void Write(std::ostream& file);			// SaveChart の最後（"}" の前）に呼ぶ。先頭に , が付く

	// ---- 「演出」の窓 ----
	//   noteTimes : ノーツの叩く時刻（ロングは先端）。逆走・止める の中にノーツがあるかの警告に使う
	void DrawPanel(const EditorTimeInfo& t, const std::vector<float>& noteTimes,
		const std::string& chartName, const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame);

	// ---- 譜面（タイムライン）の上 ----
	struct TimelineView
	{
		ImDrawList* draw = nullptr;
		float laneX0 = 0, laneX1 = 0;	// レーンの所（選んだ演出の始まりの線・色、置く前の線を出す）
		float colX0 = 0, colX1 = 0;		// 演出の列
		float yTop = 0, yBottom = 0;
		bool hovered = false;			// 譜面の上にマウスがあるか
		std::function<float(float)> timeToY;
		std::function<float(float)> yToTime;
		std::function<float(float)> snap;	// グリッドに吸着
	};
	// 演出ノーツを描いて、クリック・ドラッグを受ける。マウスを使った（ドラッグ中・演出の列の上）なら true
	//   → true の時は、譜面のノーツを置く・選ぶ処理をしない
	bool DrawTimeline(const TimelineView& v, const EditorTimeInfo& t);

	bool CapturesMouse();					// 演出ノーツをドラッグしている間 true
	bool SelectedStart(float* outStart);	// 演出を選んでいれば、その始まりの秒
	std::vector<EditorGimmick>& Items();
}
