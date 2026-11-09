#pragma once
#include "ChartObject.h" // ChartNote / ChartJudgeCircle / ChartHomingBeam の完全な定義を得るため
#include "LaneTrack.h"
#include "CameraTrack.h"
#include <string>
#include <vector>

// ============================================================
// ChartLoadResult : 譜面JSON1本分の読み込み結果。
//   ChartObjectの状態を一切参照しない、純粋なデータの入れ物。
// ============================================================
struct ChartLoadResult
{
	bool success = false;

	std::vector<ChartNote> notes;
	std::vector<ChartJudgeCircle> judgeCircles; // 常にレーン数(6)分の要素を持つ
	std::vector<ChartHomingBeam> homingBeams;

	float bpm = 120.0f;
	float offset = 0.0f;	//曲の最初の拍が何秒か　エディターのoffset
	int   beatsPerBar = 4;
	std::string audioFile;
	std::vector<LaneTrack> tracks;	//レーンの区切り（常に6本）
	std::vector<CameraKey> cameraKeys;
};

// ============================================================
// ChartLoader : charts/<name>.json を読み込んでパースするだけの機能。
//   ChartObjectのメンバには一切触れないので、単体でテスト・再利用しやすい。
// ============================================================
namespace ChartLoader
{
	// defaultJudgeCircles : JSONに"judgeCircles"の指定が無いレーンに使う既定値
	//   （呼び出し側が Init() 直後の m_JudgeCircles を渡す想定）
	ChartLoadResult Load(const std::string& name, const std::vector<ChartJudgeCircle>& defaultJudgeCircles);
}
