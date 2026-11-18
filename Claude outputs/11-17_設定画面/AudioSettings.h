#pragma once
// ============================================================
// AudioSettings : エディターの「音ズレ測定」で決めた設定を読む（ゲームのフォルダの audio_settings.json）
//   offsetMs : エディターで測った「判定をずらす量」（ms）
//   backend  : 鳴らし方（"xaudio2" / "adx" / "wasapi"）。今のゲームは XAudio2 だけなので、読むだけ（次に使う）
//   判定に使うずらしは PlayerSettings（曲選びの設定で変えられる）の方。
//   エディターで測り直すと、PlayerSettings がこちらの値を取り込む
// ============================================================
#include <string>
#include "SimpleJson.h"
#include "PlayerSettings.h"

struct AudioSettingsData
{
	std::string backend = "xaudio2";
	float offsetMs = 0.0f;
	bool loaded = false;		// ファイルを読めたか
};

namespace AudioSettings
{
	inline const AudioSettingsData& Get()
	{
		static AudioSettingsData s;
		static bool tried = false;
		if (!tried)
		{
			tried = true;
			JsonValue j;
			if (SimpleJson::ParseFile("audio_settings.json", j) && j.IsObject())
			{
				s.backend = j["backend"].AsString("xaudio2");
				s.offsetMs = j["offsetMs"].AsFloat(0.0f);
				s.loaded = true;
			}
		}
		return s;
	}

	// 判定に使う時刻（曲の時刻から、プレイヤーのタイミング調整のぶんを引く）
	inline float JudgeTime(float musicTime) { return musicTime - PlayerSettings::Get().offsetMs * 0.001f; }
}
