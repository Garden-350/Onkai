#include "PlayerSettings.h"
#include "AudioSettings.h"
#include "SimpleJson.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#undef min
#undef max

namespace fs = std::filesystem;

namespace
{
	const char* kFile = "player_settings.json";
	const char* kEditorFile = "audio_settings.json";	// エディターの「音ズレ測定」が書く

	PlayerSettingsData s_Data;
	bool s_Loaded = false;

	void Load()
	{
		s_Loaded = true;
		JsonValue j;
		bool hasFile = SimpleJson::ParseFile(kFile, j) && j.IsObject();
		if (hasFile)
		{
			s_Data.brightness = j["brightness"].AsFloat(s_Data.brightness);
			s_Data.highSpeed = j["highSpeed"].AsFloat(s_Data.highSpeed);
			s_Data.offsetMs = j["offsetMs"].AsFloat(s_Data.offsetMs);
		}

		// エディターで測り直した（audio_settings.json の方が新しい）なら、タイミングはそちらを使う
		std::error_code ec;
		if (fs::exists(kEditorFile, ec))
		{
			bool editorIsNewer = !hasFile ||
				fs::last_write_time(kEditorFile, ec) > fs::last_write_time(kFile, ec);
			if (editorIsNewer && AudioSettings::Get().loaded)
				s_Data.offsetMs = AudioSettings::Get().offsetMs;
		}

		s_Data.brightness = std::clamp(s_Data.brightness, PlayerSettings::kBrightnessMin, PlayerSettings::kBrightnessMax);
		s_Data.highSpeed = std::clamp(s_Data.highSpeed, PlayerSettings::kSpeedMin, PlayerSettings::kSpeedMax);
		s_Data.offsetMs = std::clamp(s_Data.offsetMs, PlayerSettings::kOffsetMin, PlayerSettings::kOffsetMax);
	}
}

namespace PlayerSettings
{
	PlayerSettingsData& Get()
	{
		if (!s_Loaded) Load();
		return s_Data;
	}

	void Save()
	{
		const PlayerSettingsData& d = Get();
		std::ofstream f(kFile);
		if (!f.is_open()) return;
		f << "{\n";
		f << "  \"brightness\": " << d.brightness << ",\n";
		f << "  \"highSpeed\": " << d.highSpeed << ",\n";
		f << "  \"offsetMs\": " << d.offsetMs << "\n";
		f << "}\n";
	}
}
