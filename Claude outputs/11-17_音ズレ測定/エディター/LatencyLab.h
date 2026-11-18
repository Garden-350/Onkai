#pragma once
// ============================================================
// LatencyLab : 「音ズレ測定」の窓
//   鳴らし方（XAudio2 / ADX / 自作 WASAPI）を選んで、2 つのやり方で音の遅れを数字にする
//   A 押して鳴らす：Space で四角が光って音が鳴る。押してから音が出る（とライブラリが言う）までの ms
//   B 合わせて押す：メトロノームに合わせて Space。平均で何 ms ずれたか → ゲームの判定に使う
//   B の結果は「ゲームに使う」で audio_settings.json に保存（エディターとゲームのフォルダの両方）
// ============================================================
#include <filesystem>

namespace LatencyLab
{
	void DrawPanel(const std::filesystem::path& gameRoot);	// Render の中で毎フレーム（窓を出す）
	bool CapturesKeys();									// 測定の窓を触っている間 true（エディターの Space の再生を止める）
	void Shutdown();										// 終わるとき（音の後片付け）
}
