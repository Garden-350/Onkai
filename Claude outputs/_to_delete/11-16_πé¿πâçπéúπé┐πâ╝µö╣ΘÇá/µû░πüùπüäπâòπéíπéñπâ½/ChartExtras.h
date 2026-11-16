#pragma once
// ============================================================
// ChartExtras : エディターが自分では編集しない譜面の項目を守る ＋ カメラの編集 ＋ テストプレイ
//   ・読み込んだ譜面の「知らない項目」（tracks, rolls など）を覚えておき、保存のときにそのまま書き戻す
//   ・カメラの区切り（"camera"）をエディターで編集する
//   ・テストプレイ：保存して、本番のゲーム（Sounder.exe）を指定の秒から起動する
// ============================================================
#include <filesystem>
#include <functional>
#include <ostream>
#include <string>
#include <vector>
#include "json.hpp"

struct ImDrawList;

// カメラの区切り（ゲームの CameraKey と同じ中身）
struct EditorCameraKey
{
	int fromMeasure = 1;			// 何小節目の頭で動き終わるか
	std::string preset = "default";	// カメラの名前（ゲームの CameraPresets と同じ）。"custom" なら eye / target を使う
	float blendBeats = 2.0f;		// 何拍かけて動くか
	float eye[3] = { 0.0f, 630.0f, -880.0f };		// custom のときのカメラの位置
	float target[3] = { 0.0f, -480.0f, 290.0f };	// custom のときの見る先
};

// エディターの今の時間の情報（Game から渡す）
struct EditorTimeInfo
{
	float bpm = 120.0f;
	float offset = 0.0f;		// 曲の最初の拍が何秒か（m_BaseOffset）
	int beatsPerMeasure = 4;
	float now = 0.0f;			// 今見ている時刻（再生位置かスクロール位置）
};

namespace ChartExtras
{
	// ---- 保存で消えないようにする ----
	void OnLoad(const nlohmann::json& j);		// LoadChart で JSON を読んだ直後に呼ぶ
	std::string MetaExtras();					// "meta" の中の知らない項目（先頭に , が付いた文字列）
	void WriteExtras(std::ostream& file);		// "camera" と知らない項目（先頭に , が付く）

	// ---- テストプレイ ----
	//   saveToGame : 保存してゲームのフォルダへコピーする関数（成功したら true）
	//   gameRoot   : ゲームのフォルダ（Game.cpp の kGameRoot）
	void DrawTestPlayPanel(const EditorTimeInfo& t, const std::string& chartName,
		const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame);

	//   ツールバー・ショートカットから呼ぶ用（パネルのボタンと同じこと）
	void PlayFromHere(const EditorTimeInfo& t, const std::string& chartName,
		const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame);
	void PlayFromTop(const std::string& chartName,
		const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame);

	// ---- カメラ ----
	void DrawCameraPanel(const EditorTimeInfo& t, const std::string& chartName,
		const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame);
	// 2D のタイムラインに、カメラの区切りの線と、動いている間の帯を描く
	void DrawCameraMarkers(ImDrawList* draw, float x0, float x1, float yTop, float yBottom,
		const EditorTimeInfo& t, const std::function<float(float)>& timeToY);

	std::vector<EditorCameraKey>& CameraKeys();
}
