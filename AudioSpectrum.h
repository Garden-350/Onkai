#pragma once
// ============================================================
// AudioSpectrum : Sound の LoadDynamic で読み込んだ曲(PCM)を、
//   「今鳴っている位置」付近でFFTして、周波数帯ごとの強さ(0〜1)を返すクラス。
//   描画は一切しない（描画は SpectrumRing の担当）。
//
//   使い方:
//     m_Spectrum.Init(2048, 48);                 // 最初に1回
//     m_Spectrum.Analyze(sound, levels);         // 毎フレーム。levels[帯域] に 0〜1 が入る
// ============================================================
#include <vector>

class Sound;

class AudioSpectrum
{
public:
	// fftSize   : FFTの窓サイズ（2の累乗。2048 ≒ 44.1kHzで約46ミリ秒分）
	// bandCount : 出力する帯域の数（=片側のバーの本数）
	// minHz/maxHz : 表示する周波数範囲。対数で等分する
	void Init(int fftSize = 2048, int bandCount = 48, float minHz = 40.0f, float maxHz = 14000.0f);

	// 解析できたら true。曲が止まっている・未ロードの場合は false（outLevels は全部0）
	bool Analyze(const Sound& sound, std::vector<float>& outLevels);

	int GetBandCount() const { return m_BandCount; }

private:
	void RunFFT();	// m_Re / m_Im をその場でFFTする

	int   m_FftSize = 0;
	int   m_BandCount = 0;
	float m_MinHz = 40.0f;
	float m_MaxHz = 14000.0f;

	std::vector<float> m_Window;	// ハン窓
	std::vector<float> m_Re;		// FFTの実部（入力も兼ねる）
	std::vector<float> m_Im;		// FFTの虚部
	std::vector<int>   m_BitReverse;	// ビット反転の並べ替え表
	std::vector<float> m_CosTable;	// 回転因子
	std::vector<float> m_SinTable;
};
