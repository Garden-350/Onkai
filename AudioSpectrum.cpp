#include "AudioSpectrum.h"
#include "sound.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#undef min
#undef max

namespace
{
	constexpr float kPi = 3.14159265358979f;

	// 表示の感度まわり（見た目の調整用）
	constexpr float kFloorDb = -45.0f;		// これより小さい音は0（バーが伸びない）
	constexpr float kCeilDb = -12.0f;		// これ以上の音は1（バーが最大まで伸びる）
	constexpr float kCurve = 1.2f;			// 1より大きいと、小さい音は控えめ・大きい音でグッと伸びる
	constexpr float kTiltDbPerOctave = 3.0f;	// 高音ほど元々小さいので、1オクターブごとにこれだけ持ち上げる
	constexpr float kTiltPivotHz = 1000.0f;	// 持ち上げの基準周波数（ここは±0dB）

	// フォーマットの判定結果
	enum class SampleKind { Unsupported, Int8, Int16, Int24, Int32, Float32 };

	SampleKind DetectKind(const WAVEFORMATEX& fmt)
	{
		const WORD kTagPcm = 1;
		const WORD kTagFloat = 3;
		const WORD kTagExtensible = 0xFFFE;

		WORD tag = fmt.wFormatTag;
		if (tag == kTagExtensible && fmt.cbSize >= 22)
		{
			// WAVEFORMATEXTENSIBLE の SubFormat の先頭4バイトが「実際のフォーマット番号」になっている
			const WAVEFORMATEXTENSIBLE& ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(fmt);
			tag = (WORD)ext.SubFormat.Data1;
		}

		if (fmt.nChannels == 0) return SampleKind::Unsupported;
		const int bytesPerSample = fmt.nBlockAlign / fmt.nChannels;

		if (tag == kTagFloat && bytesPerSample == 4) return SampleKind::Float32;
		if (tag == kTagPcm)
		{
			switch (bytesPerSample)
			{
			case 1: return SampleKind::Int8;
			case 2: return SampleKind::Int16;
			case 3: return SampleKind::Int24;
			case 4: return SampleKind::Int32;
			}
		}
		return SampleKind::Unsupported;
	}

	// 1フレーム分（全チャンネル）を読んで、モノラル(-1〜1)にして返す
	float ReadMono(const BYTE* frame, int channels, int bytesPerSample, SampleKind kind)
	{
		float sum = 0.0f;
		for (int ch = 0; ch < channels; ch++)
		{
			const BYTE* p = frame + ch * bytesPerSample;
			float v = 0.0f;
			switch (kind)
			{
			case SampleKind::Int8:
				v = ((int)p[0] - 128) / 128.0f;
				break;
			case SampleKind::Int16:
			{
				int16_t s; memcpy(&s, p, 2);
				v = s / 32768.0f;
				break;
			}
			case SampleKind::Int24:
			{
				int32_t s = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
				v = s / 8388608.0f;
				break;
			}
			case SampleKind::Int32:
			{
				int32_t s; memcpy(&s, p, 4);
				v = s / 2147483648.0f;
				break;
			}
			case SampleKind::Float32:
				memcpy(&v, p, 4);
				break;
			default:
				break;
			}
			sum += v;
		}
		return sum / (float)channels;
	}
}

void AudioSpectrum::Init(int fftSize, int bandCount, float minHz, float maxHz)
{
	// fftSize は2の累乗に切り上げる（FFTの都合）
	int n = 1;
	while (n < fftSize) n <<= 1;
	m_FftSize = n;
	m_BandCount = bandCount > 0 ? bandCount : 1;
	m_MinHz = minHz;
	m_MaxHz = maxHz;

	m_Window.resize(n);
	m_Re.assign(n, 0.0f);
	m_Im.assign(n, 0.0f);
	for (int i = 0; i < n; i++)
	{
		m_Window[i] = 0.5f - 0.5f * cosf(2.0f * kPi * i / (n - 1));	// ハン窓
	}

	// ビット反転表
	int bits = 0;
	while ((1 << bits) < n) bits++;
	m_BitReverse.resize(n);
	for (int i = 0; i < n; i++)
	{
		int r = 0;
		for (int b = 0; b < bits; b++)
		{
			if (i & (1 << b)) r |= 1 << (bits - 1 - b);
		}
		m_BitReverse[i] = r;
	}

	// 回転因子
	m_CosTable.resize(n / 2);
	m_SinTable.resize(n / 2);
	for (int i = 0; i < n / 2; i++)
	{
		m_CosTable[i] = cosf(2.0f * kPi * i / n);
		m_SinTable[i] = -sinf(2.0f * kPi * i / n);
	}
}

void AudioSpectrum::RunFFT()
{
	const int n = m_FftSize;

	// ビット反転で並べ替え
	for (int i = 0; i < n; i++)
	{
		int j = m_BitReverse[i];
		if (j > i)
		{
			float tr = m_Re[i]; m_Re[i] = m_Re[j]; m_Re[j] = tr;
			float ti = m_Im[i]; m_Im[i] = m_Im[j]; m_Im[j] = ti;
		}
	}

	// バタフライ演算
	for (int size = 2; size <= n; size <<= 1)
	{
		int half = size / 2;
		int step = n / size;
		for (int start = 0; start < n; start += size)
		{
			for (int k = 0; k < half; k++)
			{
				float wr = m_CosTable[k * step];
				float wi = m_SinTable[k * step];
				int a = start + k;
				int b = a + half;
				float xr = m_Re[b] * wr - m_Im[b] * wi;
				float xi = m_Re[b] * wi + m_Im[b] * wr;
				m_Re[b] = m_Re[a] - xr;
				m_Im[b] = m_Im[a] - xi;
				m_Re[a] += xr;
				m_Im[a] += xi;
			}
		}
	}
}

bool AudioSpectrum::Analyze(const Sound& sound, std::vector<float>& outLevels)
{
	outLevels.assign(m_BandCount, 0.0f);
	if (m_FftSize == 0) return false;
	if (!sound.IsDynamicPlaying()) return false;

	const BYTE* data = sound.GetDynamicData();
	const UINT32 bytes = sound.GetDynamicDataBytes();
	const WAVEFORMATEX& fmt = sound.GetDynamicFormat();
	if (data == nullptr || fmt.nBlockAlign == 0 || fmt.nSamplesPerSec == 0 || fmt.nChannels == 0) return false;

	const SampleKind kind = DetectKind(fmt);
	if (kind == SampleKind::Unsupported) return false;

	const long long totalFrames = bytes / fmt.nBlockAlign;
	if (totalFrames < m_FftSize) return false;

	const int channels = fmt.nChannels;
	const int bytesPerSample = fmt.nBlockAlign / channels;
	const float sampleRate = (float)fmt.nSamplesPerSec;

	// 「今鳴っている位置」の直前 fftSize フレームを切り出す（ループしている曲なので端は巻き戻す）
	const long long endFrame = (long long)(sound.GetDynamicPlaySeconds() * sampleRate);
	const long long startFrame = endFrame - m_FftSize;
	for (int i = 0; i < m_FftSize; i++)
	{
		long long idx = (startFrame + i) % totalFrames;
		if (idx < 0) idx += totalFrames;
		const BYTE* frame = data + idx * fmt.nBlockAlign;
		m_Re[i] = ReadMono(frame, channels, bytesPerSample, kind) * m_Window[i];
		m_Im[i] = 0.0f;
	}

	RunFFT();

	// 周波数帯ごとにまとめる（対数で等間隔）
	const int nyquistBin = m_FftSize / 2;
	const float binHz = sampleRate / (float)m_FftSize;
	const float ratio = m_MaxHz / m_MinHz;
	// ハン窓(平均0.5)と片側スペクトル(×2)を補正して「振幅1の正弦波 = 1.0」になるようにする
	const float ampScale = 4.0f / (float)m_FftSize;

	for (int b = 0; b < m_BandCount; b++)
	{
		float loHz = m_MinHz * powf(ratio, (float)b / m_BandCount);
		float hiHz = m_MinHz * powf(ratio, (float)(b + 1) / m_BandCount);

		int loBin = (int)floorf(loHz / binHz);
		int hiBin = (int)ceilf(hiHz / binHz);
		if (loBin < 1) loBin = 1;
		if (hiBin > nyquistBin - 1) hiBin = nyquistBin - 1;
		if (hiBin <= loBin) hiBin = loBin + 1;

		// 帯域内のいちばん大きい成分を採用（平均だと低音の少ないビンに引っ張られて地味になる）
		float peak = 0.0f;
		for (int k = loBin; k < hiBin && k < nyquistBin; k++)
		{
			float mag = sqrtf(m_Re[k] * m_Re[k] + m_Im[k] * m_Im[k]);
			if (mag > peak) peak = mag;
		}

		float amp = peak * ampScale;
		float db = 20.0f * log10f(amp + 1e-9f);

		// 高音ほど持ち上げる
		float centerHz = sqrtf(loHz * hiHz);
		db += kTiltDbPerOctave * log2f(centerHz / kTiltPivotHz);

		float level = (db - kFloorDb) / (kCeilDb - kFloorDb);
		if (level < 0.0f) level = 0.0f;
		if (level > 1.0f) level = 1.0f;
		outLevels[b] = powf(level, kCurve);
	}
	return true;
}
