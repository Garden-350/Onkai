#include "pch.h"
#include "AdxMusic.h"
#include <cri_adx2le.h>			// CRI ADX LE（プロジェクトの「追加のインクルード ディレクトリ」に cri\pc\include を足す）
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

// ライブラリをつなぐ（「追加のライブラリ ディレクトリ」に cri\pc\libs\x64 を足す）
// 実行するときは cri_ware_pcx64_le.dll が exe の隣に必要（ビルド後イベントでコピー）
#pragma comment(lib, "cri_ware_pcx64_le_import.lib")

namespace
{
	bool s_Ready = false;									// Init が成功したか
	CriAtomExVoicePoolHn s_WavePool = nullptr;				// WAV 用のボイス（音を鳴らす入れ物）
	CriAtomExVoicePoolHn s_StdPool = nullptr;				// HCA / ADX 用のボイス
	CriAtomExPlayerHn s_Player = nullptr;					// 曲を鳴らすプレーヤー
	CriAtomExPlaybackId s_Playback = CRIATOMEX_INVALID_PLAYBACK_ID;	// 今鳴らしている音の番号
	std::string s_Path;										// 鳴らす曲のファイル
	CriAtomExFormat s_Format = CRIATOMEX_FORMAT_NONE;		// 曲の形式
	float s_StartSec = 0.0f;								// どこから鳴らしたか（秒）

	// ADX が使うメモリ（malloc / free をそのまま使う）
	void* AdxAlloc(void* obj, CriUint32 size) { (void)obj; return malloc(size); }
	void AdxFree(void* obj, void* ptr) { (void)obj; free(ptr); }

	// ADX のエラーは VS の「出力」に出す
	void AdxError(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray)
	{
		(void)parray;
		const CriChar8* msg = criErr_ConvertIdToMessage(errid, p1, p2);
		OutputDebugStringA("[ADX] ");
		OutputDebugStringA(msg ? msg : errid);
		OutputDebugStringA("\n");
	}

	std::string LowerExt(const std::string& path)
	{
		std::string ext = std::filesystem::path(path).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return ext;
	}
}

namespace AdxMusic
{
	bool Init()
	{
		if (s_Ready) return true;
		criErr_SetCallback(AdxError);
		criAtomEx_SetUserAllocator(AdxAlloc, AdxFree, NULL);

		// ライブラリの初期化（WASAPI：Windows の低遅延の音の出口を使う）
		CriAtomExConfig_WASAPI libConfig;
		criAtomEx_SetDefaultConfig_WASAPI(&libConfig);
		criAtomEx_Initialize_WASAPI(&libConfig, NULL, 0);

		// ボイスプール：音を鳴らすための入れ物。曲は 1 つしか鳴らさないので 1 つずつ
		//   streaming_flag = TRUE：ファイルを少しずつ読みながら鳴らす（長い曲でもメモリを食わない）
		CriAtomExWaveVoicePoolConfig waveConfig;
		criAtomExVoicePool_SetDefaultConfigForWaveVoicePool(&waveConfig);
		waveConfig.num_voices = 1;
		waveConfig.player_config.streaming_flag = CRI_TRUE;
		waveConfig.player_config.max_channels = 2;				// ステレオまで
		waveConfig.player_config.max_sampling_rate = 48000;	// 48kHz まで（44.1kHz もこれで鳴る）
		s_WavePool = criAtomExVoicePool_AllocateWaveVoicePool(&waveConfig, NULL, 0);

		CriAtomExStandardVoicePoolConfig stdConfig;
		criAtomExVoicePool_SetDefaultConfigForStandardVoicePool(&stdConfig);
		stdConfig.num_voices = 1;
		stdConfig.player_config.streaming_flag = CRI_TRUE;
		stdConfig.player_config.max_sampling_rate = 48000;
		s_StdPool = criAtomExVoicePool_AllocateStandardVoicePool(&stdConfig, NULL, 0);

		// プレーヤー：enable_audio_synced_timer = TRUE が「音に合わせた時刻」を取るためのスイッチ
		//   （そのかわり、このプレーヤーでは速さ・ピッチを変えられない）
		CriAtomExPlayerConfig playerConfig;
		criAtomExPlayer_SetDefaultConfig(&playerConfig);
		playerConfig.max_path = 260;							// ファイルの名前の長さの上限（SetFile に必要）
		playerConfig.enable_audio_synced_timer = CRI_TRUE;
		s_Player = criAtomExPlayer_Create(&playerConfig, NULL, 0);

		s_Ready = (s_WavePool != nullptr && s_StdPool != nullptr && s_Player != nullptr);
		if (!s_Ready)
		{
			OutputDebugStringA("[ADX] Init failed\n");
			Finalize();
		}
		return s_Ready;
	}

	void Finalize()
	{
		if (s_Player) { criAtomExPlayer_Stop(s_Player); criAtomExPlayer_Destroy(s_Player); s_Player = nullptr; }
		if (s_WavePool) { criAtomExVoicePool_Free(s_WavePool); s_WavePool = nullptr; }
		if (s_StdPool) { criAtomExVoicePool_Free(s_StdPool); s_StdPool = nullptr; }
		criAtomEx_Finalize_WASAPI();
		s_Ready = false;
		s_Playback = CRIATOMEX_INVALID_PLAYBACK_ID;
	}

	void Update()
	{
		if (s_Ready) criAtomEx_ExecuteMain();
	}

	bool Load(const std::string& path)
	{
		Stop();
		s_Path.clear();
		s_Format = CRIATOMEX_FORMAT_NONE;
		if (!s_Ready || path.empty()) return false;

		std::string ext = LowerExt(path);
		if (ext == ".wav") s_Format = CRIATOMEX_FORMAT_WAVE;
		else if (ext == ".hca") s_Format = CRIATOMEX_FORMAT_HCA;
		else if (ext == ".adx") s_Format = CRIATOMEX_FORMAT_ADX;
		else return false;		// MP3 など：ADX では鳴らせない（今まで通り XAudio で鳴らす）

		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) { s_Format = CRIATOMEX_FORMAT_NONE; return false; }
		s_Path = path;
		return true;
	}

	void Unload()
	{
		Stop();
		s_Path.clear();
		s_Format = CRIATOMEX_FORMAT_NONE;
	}

	bool IsLoaded() { return s_Ready && !s_Path.empty(); }

	void Play(float fromSec)
	{
		if (!IsLoaded()) return;
		criAtomExPlayer_Stop(s_Player);
		criAtomExPlayer_Pause(s_Player, CRI_FALSE);				// 前の一時停止が残らないように
		criAtomExPlayer_SetFile(s_Player, NULL, s_Path.c_str());	// どのファイルを
		criAtomExPlayer_SetFormat(s_Player, s_Format);				// どの形式で
		criAtomExPlayer_SetNumChannels(s_Player, 2);
		criAtomExPlayer_SetSamplingRate(s_Player, 48000);			// 上限（実際はファイルの値で鳴る）
		s_StartSec = std::max(0.0f, fromSec);
		criAtomExPlayer_SetStartTime(s_Player, (CriSint64)(s_StartSec * 1000.0f));	// 何ミリ秒目から
		s_Playback = criAtomExPlayer_Start(s_Player);
	}

	void Stop()
	{
		if (s_Player) criAtomExPlayer_Stop(s_Player);
		s_Playback = CRIATOMEX_INVALID_PLAYBACK_ID;
	}

	void Pause(bool on)
	{
		if (s_Player) criAtomExPlayer_Pause(s_Player, on ? CRI_TRUE : CRI_FALSE);
	}

	bool IsPlaying()
	{
		if (!s_Player) return false;
		CriAtomExPlayerStatus st = criAtomExPlayer_GetStatus(s_Player);
		return st == CRIATOMEXPLAYER_STATUS_PREP || st == CRIATOMEXPLAYER_STATUS_PLAYING;
	}

	float GetTime()
	{
		if (s_Playback == CRIATOMEX_INVALID_PLAYBACK_ID) return -1.0f;
		// 実際に鳴らしたサンプル数に合わせた「鳴らし始めてからの時間」（マイクロ秒）
		CriSint64 us = criAtomExPlayback_GetTimeSyncedWithAudioMicro(s_Playback);
		if (us < 0) return -1.0f;			// 準備中・もう終わった
		return s_StartSec + (float)((double)us / 1000000.0);	// 鳴らし始めた位置を足して「曲の何秒目か」にする
	}

	void SetVolume(float volume)
	{
		if (!s_Player) return;
		criAtomExPlayer_SetVolume(s_Player, std::clamp(volume, 0.0f, 1.0f));
		criAtomExPlayer_UpdateAll(s_Player);		// 鳴っている音にも反映
	}
}
