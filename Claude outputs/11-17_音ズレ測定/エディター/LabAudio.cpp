#include "pch.h"
#include "LabAudio.h"
#include "AdxMusic.h"
#include <cri_adx2le.h>
#include <xaudio2.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "avrt.lib")

using Microsoft::WRL::ComPtr;

// ============================================================
// 共通
// ============================================================
double NowSec()
{
	static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return (double)c.QuadPart / (double)freq.QuadPart;
}

namespace
{
	constexpr int kRate = 48000;			// XAudio2 と ADX で使うサンプリング周波数
	constexpr double kSearchSec = 0.5;		// 押してから何秒の間、鳴り始めの時刻を探すか

	// 1 回分のクリック（1.5kHz・25ms・すぐ小さくなる）を out[at] から足す
	void AddClick(std::vector<float>& out, size_t at, int rate, float freqHz, float volume)
	{
		const int len = rate * 25 / 1000;
		const float twoPi = 6.2831853f;
		for (int i = 0; i < len && at + i < out.size(); i++)
		{
			float t = (float)i / rate;
			float env = std::exp(-t * 180.0f);			// 立ち上がりが鋭く、すぐ消える
			out[at + i] += volume * env * std::sin(twoPi * freqHz * t);
		}
	}

	std::vector<int16_t> ToInt16(const std::vector<float>& f)
	{
		std::vector<int16_t> o(f.size());
		for (size_t i = 0; i < f.size(); i++)
			o[i] = (int16_t)std::lround(std::clamp(f[i], -1.0f, 1.0f) * 32767.0f);
		return o;
	}

	// WAV ファイルの形（RIFF の見出し ＋ 16bit モノラル）をメモリの上に作る（ADX のメモリ再生用）
	std::vector<uint8_t> MakeWavImage(const std::vector<float>& mono, int rate)
	{
		std::vector<int16_t> pcm = ToInt16(mono);
		const uint32_t dataBytes = (uint32_t)(pcm.size() * 2);
		std::vector<uint8_t> w(44 + dataBytes);
		auto put32 = [&](size_t at, uint32_t v) { std::memcpy(&w[at], &v, 4); };
		auto put16 = [&](size_t at, uint16_t v) { std::memcpy(&w[at], &v, 2); };
		std::memcpy(&w[0], "RIFF", 4); put32(4, 36 + dataBytes); std::memcpy(&w[8], "WAVE", 4);
		std::memcpy(&w[12], "fmt ", 4); put32(16, 16); put16(20, 1); put16(22, 1);
		put32(24, (uint32_t)rate); put32(28, (uint32_t)rate * 2); put16(32, 2); put16(34, 16);
		std::memcpy(&w[36], "data", 4); put32(40, dataBytes);
		std::memcpy(&w[44], pcm.data(), dataBytes);
		return w;
	}
}

std::vector<float> MakeClickSound(int rate, float seconds)
{
	std::vector<float> o((size_t)(rate * seconds), 0.0f);
	AddClick(o, 0, rate, 1500.0f, 0.8f);
	return o;
}

std::vector<float> MakeMetronome(int rate, float bpm, int beats, float leadSec)
{
	const double spb = 60.0 / bpm;
	std::vector<float> o((size_t)(rate * (leadSec + spb * beats + 0.5)), 0.0f);
	for (int b = 0; b < beats; b++)
	{
		size_t at = (size_t)std::llround((leadSec + spb * b) * rate);
		bool head = (b % 4 == 0);				// 小節の頭は高い音
		AddClick(o, at, rate, head ? 2000.0f : 1300.0f, head ? 0.8f : 0.6f);
	}
	return o;
}

// ============================================================
// XAudio2
//   SamplesPlayed（ボイスが処理したサンプル数）から「鳴り始めた時刻」を逆算し、
//   エンジンの遅れ（CurrentLatencyInSamples）を足して「聞こえた時刻」にする
// ============================================================
namespace
{
	class LabXAudio2 : public LabAudio
	{
	public:
		LabXAudio2()
		{
			if (FAILED(XAudio2Create(&m_X, 0, XAUDIO2_DEFAULT_PROCESSOR))) { m_Error = "XAudio2Create failed"; return; }
			if (FAILED(m_X->CreateMasteringVoice(&m_Master))) { m_Error = "CreateMasteringVoice failed"; return; }
			XAUDIO2_VOICE_DETAILS d = {};
			m_Master->GetVoiceDetails(&d);
			m_MasterRate = d.InputSampleRate;
			m_Click = ToInt16(MakeClickSound(kRate, 0.3f));
		}
		~LabXAudio2() override
		{
			if (m_ClickVoice) m_ClickVoice->DestroyVoice();
			if (m_TrackVoice) m_TrackVoice->DestroyVoice();
			if (m_Master) m_Master->DestroyVoice();
			if (m_X) m_X->Release();
		}
		const char* Name() const override { return "XAudio2"; }

		void PlayClick() override
		{
			if (!Ok()) return;
			m_ClickVoice = Restart(m_ClickVoice, m_Click);	// ボイスを作り直す（SamplesPlayed を 0 から数えるため）
			m_ClickPress = NowSec();
			m_ClickBest = -1.0;
		}
		double ClickHeardAt() override
		{
			if (!m_ClickVoice) return -1.0;
			UpdateBest(m_ClickVoice, m_ClickPress, m_ClickBest);
			return (m_ClickBest < 0.0) ? -1.0 : m_ClickBest + EngineLatencySec();
		}

		int TrackRate() const override { return kRate; }
		void PlayTrack(const std::vector<float>& mono) override
		{
			if (!Ok()) return;
			m_Track = ToInt16(mono);
			m_TrackVoice = Restart(m_TrackVoice, m_Track);
			m_TrackPress = NowSec();
			m_TrackBest = -1.0;
		}
		void StopTrack() override
		{
			if (m_TrackVoice) { m_TrackVoice->DestroyVoice(); m_TrackVoice = nullptr; }
		}
		double HeardTrackSec() override
		{
			if (!m_TrackVoice) return -1.0;
			UpdateBest(m_TrackVoice, m_TrackPress, m_TrackBest, 1e9);
			if (m_TrackBest < 0.0) return -1.0;
			return NowSec() - m_TrackBest - EngineLatencySec();		// 鳴り始めからの時間 − 遅れ ＝ 今聞こえている所
		}
		double ReportedLatencyMs() override { return Ok() ? EngineLatencySec() * 1000.0 : -1.0; }

	private:
		IXAudio2SourceVoice* Restart(IXAudio2SourceVoice* old, const std::vector<int16_t>& pcm)
		{
			if (old) old->DestroyVoice();
			WAVEFORMATEX wf = {};
			wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 1; wf.nSamplesPerSec = kRate;
			wf.wBitsPerSample = 16; wf.nBlockAlign = 2; wf.nAvgBytesPerSec = kRate * 2;
			IXAudio2SourceVoice* v = nullptr;
			if (FAILED(m_X->CreateSourceVoice(&v, &wf))) return nullptr;
			XAUDIO2_BUFFER b = {};
			b.AudioBytes = (UINT32)(pcm.size() * 2);
			b.pAudioData = reinterpret_cast<const BYTE*>(pcm.data());
			b.Flags = XAUDIO2_END_OF_STREAM;
			v->SubmitSourceBuffer(&b);
			v->Start();
			return v;
		}
		// 「今 − 処理済みサンプル数」は、鳴り始めた時刻より少し後ろにぶれる（10ms ごとにまとめて進むため）
		// 何回か測って一番早い物を使うと、本当の鳴り始めに近づく
		static void UpdateBest(IXAudio2SourceVoice* v, double press, double& best, double window = kSearchSec)
		{
			double now = NowSec();
			if (now - press > window) return;
			XAUDIO2_VOICE_STATE st = {};
			v->GetState(&st);
			if (st.SamplesPlayed == 0) return;
			double start = now - (double)st.SamplesPlayed / kRate;
			if (best < 0.0 || start < best) best = start;
		}
		double EngineLatencySec()
		{
			XAUDIO2_PERFORMANCE_DATA pd = {};
			m_X->GetPerformanceData(&pd);
			return (m_MasterRate > 0) ? (double)pd.CurrentLatencyInSamples / m_MasterRate : 0.0;
		}

		IXAudio2* m_X = nullptr;
		IXAudio2MasteringVoice* m_Master = nullptr;
		IXAudio2SourceVoice* m_ClickVoice = nullptr;
		IXAudio2SourceVoice* m_TrackVoice = nullptr;
		UINT32 m_MasterRate = 48000;
		std::vector<int16_t> m_Click, m_Track;
		double m_ClickPress = 0.0, m_ClickBest = -1.0;
		double m_TrackPress = 0.0, m_TrackBest = -1.0;
	};
}
std::unique_ptr<LabAudio> CreateLabXAudio2() { return std::make_unique<LabXAudio2>(); }

// ============================================================
// ADX
//   criAtomExPlayback_GetTimeSyncedWithAudioMicro（実際に鳴った分に合わせた経過時間）から逆算する
// ============================================================
namespace
{
	constexpr CriAtomExVoicePoolIdentifier kLabPoolId = 77;	// 曲（AdxMusic）とは別のボイスを使う

	class LabAdx : public LabAudio
	{
	public:
		LabAdx()
		{
			if (!AdxMusic::IsReady()) { m_Error = "ADX is not initialized (AdxMusic::Init)"; return; }
			CriAtomExWaveVoicePoolConfig vc;
			criAtomExVoicePool_SetDefaultConfigForWaveVoicePool(&vc);
			vc.identifier = kLabPoolId;
			vc.num_voices = 2;
			vc.player_config.streaming_flag = CRI_FALSE;		// メモリの上の音を鳴らす
			vc.player_config.max_channels = 1;
			vc.player_config.max_sampling_rate = kRate;
			m_Pool = criAtomExVoicePool_AllocateWaveVoicePool(&vc, NULL, 0);
			m_ClickP = MakePlayer();
			m_TrackP = MakePlayer();
			if (!m_Pool || !m_ClickP || !m_TrackP) { m_Error = "ADX player create failed"; return; }
			m_ClickWav = MakeWavImage(MakeClickSound(kRate, 0.3f), kRate);
		}
		~LabAdx() override
		{
			if (m_ClickP) { criAtomExPlayer_Stop(m_ClickP); criAtomExPlayer_Destroy(m_ClickP); }
			if (m_TrackP) { criAtomExPlayer_Stop(m_TrackP); criAtomExPlayer_Destroy(m_TrackP); }
			if (m_Pool) criAtomExVoicePool_Free(m_Pool);
		}
		const char* Name() const override { return "ADX"; }

		void PlayClick() override
		{
			if (!Ok()) return;
			m_ClickId = Start(m_ClickP, m_ClickWav);
			m_ClickPress = NowSec();
			m_ClickBest = -1.0;
		}
		double ClickHeardAt() override
		{
			if (m_ClickId == CRIATOMEX_INVALID_PLAYBACK_ID) return -1.0;
			double now = NowSec();
			if (now - m_ClickPress < kSearchSec)
			{
				CriSint64 us = criAtomExPlayback_GetTimeSyncedWithAudioMicro(m_ClickId);
				if (us >= 0)
				{
					double start = now - us / 1e6;
					if (m_ClickBest < 0.0 || start < m_ClickBest) m_ClickBest = start;
				}
			}
			return m_ClickBest;
		}

		int TrackRate() const override { return kRate; }
		void PlayTrack(const std::vector<float>& mono) override
		{
			if (!Ok()) return;
			criAtomExPlayer_Stop(m_TrackP);
			m_TrackWav = MakeWavImage(mono, kRate);
			m_TrackId = Start(m_TrackP, m_TrackWav);
		}
		void StopTrack() override
		{
			if (m_TrackP) criAtomExPlayer_Stop(m_TrackP);
			m_TrackId = CRIATOMEX_INVALID_PLAYBACK_ID;
		}
		double HeardTrackSec() override
		{
			if (m_TrackId == CRIATOMEX_INVALID_PLAYBACK_ID) return -1.0;
			CriSint64 us = criAtomExPlayback_GetTimeSyncedWithAudioMicro(m_TrackId);
			return (us < 0) ? -1.0 : us / 1e6;
		}
		double ReportedLatencyMs() override { return -1.0; }	// ADX は時刻の中で補正しているので、別には出さない

	private:
		CriAtomExPlayerHn MakePlayer()
		{
			CriAtomExPlayerConfig pc;
			criAtomExPlayer_SetDefaultConfig(&pc);
			pc.enable_audio_synced_timer = CRI_TRUE;
			CriAtomExPlayerHn p = criAtomExPlayer_Create(&pc, NULL, 0);
			if (p) criAtomExPlayer_SetVoicePoolIdentifier(p, kLabPoolId);
			return p;
		}
		static CriAtomExPlaybackId Start(CriAtomExPlayerHn p, std::vector<uint8_t>& wav)
		{
			criAtomExPlayer_Stop(p);
			criAtomExPlayer_SetData(p, wav.data(), (CriSint32)wav.size());
			criAtomExPlayer_SetFormat(p, CRIATOMEX_FORMAT_WAVE);
			criAtomExPlayer_SetNumChannels(p, 1);
			criAtomExPlayer_SetSamplingRate(p, kRate);
			return criAtomExPlayer_Start(p);
		}

		CriAtomExVoicePoolHn m_Pool = nullptr;
		CriAtomExPlayerHn m_ClickP = nullptr, m_TrackP = nullptr;
		CriAtomExPlaybackId m_ClickId = CRIATOMEX_INVALID_PLAYBACK_ID, m_TrackId = CRIATOMEX_INVALID_PLAYBACK_ID;
		std::vector<uint8_t> m_ClickWav, m_TrackWav;	// 鳴っている間は消さない（ADX はコピーせずに読む）
		double m_ClickPress = 0.0, m_ClickBest = -1.0;
	};
}
std::unique_ptr<LabAudio> CreateLabAdx() { return std::make_unique<LabAdx>(); }

// ============================================================
// 自作：WASAPI を直接使う
//   ・共有モード ＋ イベント駆動。IAudioClient3 が使えれば、Windows が許す一番短い周期にする
//   ・専用のスレッドが「空いた分だけ音を書く」をくり返す。クリックは「何フレーム目から書いたか」を覚える
//   ・IAudioClock::GetPosition が「今スピーカーから出ているフレーム」と「その時の QPC」を返すので、
//     クリックのフレームが出る時刻を正確に計算できる
// ============================================================
namespace
{
	class LabWasapi : public LabAudio
	{
	public:
		LabWasapi()
		{
			if (!Open()) { Close(); return; }
			m_Click = MakeClickSound(m_Rate, 0.3f);
			m_Run = true;
			m_Thread = std::thread([this] { RenderLoop(); });
		}
		~LabWasapi() override { Close(); }
		const char* Name() const override { return "WASAPI (self-made)"; }

		void PlayClick() override
		{
			if (!Ok()) return;
			m_ClickFrame = -1;
			m_ClickReq = true;			// 次に書くときに、頭から足してもらう
		}
		double ClickHeardAt() override
		{
			long long f = m_ClickFrame;
			return (f < 0) ? -1.0 : FrameHeardAt((double)f);
		}

		int TrackRate() const override { return m_Rate; }
		void PlayTrack(const std::vector<float>& mono) override
		{
			if (!Ok()) return;
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_PendingTrack = std::make_shared<std::vector<float>>(mono);
			}
			m_TrackFrame = -1;
			m_TrackReq = true;
		}
		void StopTrack() override { m_TrackStopReq = true; m_TrackFrame = -1; }
		double HeardTrackSec() override
		{
			long long f = m_TrackFrame;
			if (f < 0 || !m_Clock) return -1.0;
			double posFrames, qpcSec;
			if (!DevicePosition(posFrames, qpcSec)) return -1.0;
			double nowFrames = posFrames + (NowSec() - qpcSec) * m_Rate;	// GetPosition の時から今までに進んだ分を足す
			return (nowFrames - (double)f) / m_Rate;
		}
		double ReportedLatencyMs() override { return m_LatencyMs; }

	private:
		bool Open()
		{
			ComPtr<IMMDeviceEnumerator> en;
			if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) { m_Error = "MMDeviceEnumerator failed"; return false; }
			if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &m_Device))) { m_Error = "no audio device"; return false; }

			UINT32 periodFrames = 0;
			// Windows 10 以降：IAudioClient3 で一番短い周期にする（低遅延）
			ComPtr<IAudioClient3> c3;
			if (SUCCEEDED(m_Device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, &c3)) &&
				SUCCEEDED(c3->GetMixFormat(&m_Format)))
			{
				UINT32 def = 0, fund = 0, minP = 0, maxP = 0;
				if (SUCCEEDED(c3->GetSharedModeEnginePeriod(m_Format, &def, &fund, &minP, &maxP)) &&
					SUCCEEDED(c3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minP, m_Format, nullptr)))
				{
					m_Client = c3;
					periodFrames = minP;
				}
				else
				{
					CoTaskMemFree(m_Format);
					m_Format = nullptr;
				}
			}
			// だめなら普通の IAudioClient（周期は Windows のおまかせ）
			if (!m_Client)
			{
				if (FAILED(m_Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &m_Client))) { m_Error = "IAudioClient failed"; return false; }
				if (FAILED(m_Client->GetMixFormat(&m_Format))) { m_Error = "GetMixFormat failed"; return false; }
				REFERENCE_TIME defP = 0, minP = 0;
				m_Client->GetDevicePeriod(&defP, &minP);
				if (FAILED(m_Client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, m_Format, nullptr)))
				{
					m_Error = "IAudioClient::Initialize failed"; return false;
				}
				periodFrames = (UINT32)(defP * m_Format->nSamplesPerSec / 10000000);
			}

			m_Rate = (int)m_Format->nSamplesPerSec;
			m_Channels = m_Format->nChannels;
			m_Bits = m_Format->wBitsPerSample;
			m_IsFloat = (m_Format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
				(m_Format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
					reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_Format)->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT);
			if (!(m_IsFloat && m_Bits == 32) && !(!m_IsFloat && m_Bits == 16)) { m_Error = "unsupported mix format"; return false; }

			m_Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
			if (!m_Event || FAILED(m_Client->SetEventHandle(m_Event))) { m_Error = "SetEventHandle failed"; return false; }
			if (FAILED(m_Client->GetService(IID_PPV_ARGS(&m_Render)))) { m_Error = "IAudioRenderClient failed"; return false; }
			if (FAILED(m_Client->GetService(IID_PPV_ARGS(&m_Clock)))) { m_Error = "IAudioClock failed"; return false; }
			m_Clock->GetFrequency(&m_ClockFreq);

			REFERENCE_TIME lat = 0;
			m_Client->GetStreamLatency(&lat);
			m_LatencyMs = lat / 10000.0 + periodFrames * 1000.0 / m_Rate;	// Windows が言う遅れ ＋ 1 周期

			// 始める前に、入れ物を無音でうめておく（スピーカーの位置と「書いたフレーム数」を最初からそろえるため）
			UINT32 bufferFrames = 0;
			m_Client->GetBufferSize(&bufferFrames);
			BYTE* data = nullptr;
			if (SUCCEEDED(m_Render->GetBuffer(bufferFrames, &data)))
			{
				m_Render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
				m_Written = bufferFrames;
			}

			if (FAILED(m_Client->Start())) { m_Error = "Start failed"; return false; }
			return true;
		}

		void Close()
		{
			m_Run = false;
			if (m_Event) SetEvent(m_Event);
			if (m_Thread.joinable()) m_Thread.join();
			if (m_Client) m_Client->Stop();
			m_Render.Reset();
			m_Clock.Reset();
			m_Client.Reset();
			m_Device.Reset();
			if (m_Format) { CoTaskMemFree(m_Format); m_Format = nullptr; }
			if (m_Event) { CloseHandle(m_Event); m_Event = nullptr; }
		}

		// 今スピーカーから出ているフレームと、その時の時刻（QPC 秒）
		bool DevicePosition(double& frames, double& qpcSec)
		{
			UINT64 pos = 0, qpc = 0;
			if (FAILED(m_Clock->GetPosition(&pos, &qpc)) || m_ClockFreq == 0) return false;
			frames = (double)pos * m_Rate / (double)m_ClockFreq;
			qpcSec = qpc * 1e-7;			// 100 ナノ秒単位 → 秒
			return true;
		}
		// frame 番目のフレームがスピーカーから出る時刻（QPC 秒）
		double FrameHeardAt(double frame)
		{
			double posFrames, qpcSec;
			if (!DevicePosition(posFrames, qpcSec)) return -1.0;
			return qpcSec + (frame - posFrames) / m_Rate;
		}

		// 音を書くスレッド
		void RenderLoop()
		{
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			DWORD taskIndex = 0;
			HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);	// 音用の優先度にしてもらう
			UINT32 bufferFrames = 0;
			m_Client->GetBufferSize(&bufferFrames);
			size_t clickPos = 0, trackPos = 0;
			bool clickOn = false, trackOn = false;
			std::shared_ptr<std::vector<float>> track;

			while (m_Run)
			{
				WaitForSingleObject(m_Event, 100);
				if (!m_Run) break;
				UINT32 padding = 0;
				if (FAILED(m_Client->GetCurrentPadding(&padding))) continue;
				UINT32 avail = bufferFrames - padding;			// 空いている分
				if (avail == 0) continue;
				BYTE* data = nullptr;
				if (FAILED(m_Render->GetBuffer(avail, &data))) continue;

				// 頼まれていた物を、このかたまりの頭から始める
				if (m_ClickReq.exchange(false)) { clickPos = 0; clickOn = true; m_ClickFrame = m_Written; }
				if (m_TrackStopReq.exchange(false)) { trackOn = false; track.reset(); }
				if (m_TrackReq.exchange(false))
				{
					std::lock_guard<std::mutex> lock(m_Mutex);
					track = m_PendingTrack;
					trackPos = 0; trackOn = (track != nullptr); m_TrackFrame = m_Written;
				}

				for (UINT32 i = 0; i < avail; i++)
				{
					float s = 0.0f;
					if (clickOn) { s += m_Click[clickPos++]; if (clickPos >= m_Click.size()) clickOn = false; }
					if (trackOn) { s += (*track)[trackPos++]; if (trackPos >= track->size()) trackOn = false; }
					s = std::clamp(s, -1.0f, 1.0f);
					for (int c = 0; c < m_Channels; c++)
					{
						size_t idx = (size_t)i * m_Channels + c;
						if (m_IsFloat) reinterpret_cast<float*>(data)[idx] = s;
						else reinterpret_cast<int16_t*>(data)[idx] = (int16_t)(s * 32767.0f);
					}
				}
				m_Render->ReleaseBuffer(avail, 0);
				m_Written += avail;
			}
			if (task) AvRevertMmThreadCharacteristics(task);
			CoUninitialize();
		}

		ComPtr<IMMDevice> m_Device;
		ComPtr<IAudioClient> m_Client;
		ComPtr<IAudioRenderClient> m_Render;
		ComPtr<IAudioClock> m_Clock;
		WAVEFORMATEX* m_Format = nullptr;
		HANDLE m_Event = nullptr;
		UINT64 m_ClockFreq = 0;
		int m_Rate = 48000, m_Channels = 2, m_Bits = 32;
		bool m_IsFloat = true;
		double m_LatencyMs = -1.0;

		std::thread m_Thread;
		std::atomic<bool> m_Run{ false };
		std::atomic<bool> m_ClickReq{ false }, m_TrackReq{ false }, m_TrackStopReq{ false };
		std::atomic<long long> m_ClickFrame{ -1 }, m_TrackFrame{ -1 };	// クリック・トラックを書き始めたフレーム
		long long m_Written = 0;										// 今までに書いたフレーム数（スレッドだけが使う）
		std::vector<float> m_Click;
		std::mutex m_Mutex;
		std::shared_ptr<std::vector<float>> m_PendingTrack;
	};
}
std::unique_ptr<LabAudio> CreateLabWasapi() { return std::make_unique<LabWasapi>(); }
