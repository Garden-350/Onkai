#pragma once
// ============================================================
// PlayerSettings : プレイヤーが変えられる設定（曲選びの Space で開く「設定」の中身）
//   ・ゲームのフォルダの player_settings.json に保存。次に起動しても残る
//   ・タイミング（offsetMs）は、エディターの「音ズレ測定」で保存した audio_settings.json の方が新しければ、そちらを使う
// ============================================================

struct PlayerSettingsData
{
	float brightness = 1.0f;	// 背景の明るさ（0〜1。1 がそのまま）
	float highSpeed = 1.0f;		// ノーツの速さ（ハイスピード）
	float offsetMs = 0.0f;		// タイミング調整：判定をずらす ms（＋なら遅めに押しても合う）
};

namespace PlayerSettings
{
	// 変えられる範囲と、1 回で動く量
	constexpr float kBrightnessMin = 0.0f, kBrightnessMax = 1.0f, kBrightnessStep = 0.05f;
	constexpr float kSpeedMin = 0.1f, kSpeedMax = 9.9f, kSpeedStep = 0.1f;
	constexpr float kOffsetMin = -300.0f, kOffsetMax = 300.0f, kOffsetStep = 1.0f, kOffsetBigStep = 10.0f;

	PlayerSettingsData& Get();	// 最初に呼んだ時にファイルを読む
	void Save();				// player_settings.json に書く
}
