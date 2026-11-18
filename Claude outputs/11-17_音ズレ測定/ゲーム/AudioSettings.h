#pragma once
// ============================================================
// AudioSettings : エディターの「音ズレ測定」で決めた設定を読む（ゲームのフォルダの audio_settings.json）
//   offsetMs : 判定をずらす量（ms）。押した時刻からこの分を引いて判定する（＋なら遅めに押しても合う）
//   backend  : 鳴らし方（"xaudio2" / "adx" / "wasapi"）。今のゲームは XAudio2 だけなので、読むだけ（次に使う）
//   ファイルが無ければ offsetMs = 0（今まで通り）
// ============================================================
#include <string>
#include "SimpleJson.h"

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

	// 判定に使う時刻（曲の時刻から、ずらす量を引く）
	inline float JudgeTime(float musicTime) { return musicTime - Get().offsetMs * 0.001f; }
}
