#pragma once
// ============================================================
// LabAudio : 音ズレ測定用の「鳴らし方」3 つ（XAudio2 / ADX / 自作 WASAPI）
//   どれも同じ形（LabAudio）で使えるようにしてある。音ズレ測定の部屋（LatencyLab）から使う
//
//   時刻は全部「QPC 秒」（QueryPerformanceCounter をパソコンの起動からの秒にした物。NowSec()）でそろえる
//   ・ClickHeardAt()  : クリック音が「スピーカーから出た（はず）」時刻
//   ・HeardTrackSec() : 今スピーカーから出ている所が、トラックの何秒目か
//   「はず」なのは、パソコンはスピーカーの空気の振動までは分からないから。
//   それぞれのライブラリが「ここまで鳴らした」と言う位置から計算している
// ============================================================
#include <memory>
#include <string>
#include <vector>

double NowSec();	// 今の時刻（QPC 秒）

// 音を作る（モノラル・-1〜1 の float）
std::vector<float> MakeClickSound(int rate, float seconds);		// 頭にクリック（25ms）。後ろは無音
std::vector<float> MakeMetronome(int rate, float bpm, int beats, float leadSec);	// leadSec 秒の無音のあと、拍の頭にクリック

class LabAudio
{
public:
	virtual ~LabAudio() = default;
	virtual const char* Name() const = 0;
	bool Ok() const { return m_Error.empty(); }			// 使えるか
	const std::string& Error() const { return m_Error; }	// 使えない理由

	// ---- A：押して鳴らす ----
	virtual void PlayClick() = 0;						// 今すぐクリックを鳴らす
	virtual double ClickHeardAt() = 0;					// 最後のクリックが聞こえた時刻（QPC 秒）。まだ分からなければ -1

	// ---- B：合わせて押す ----
	virtual int TrackRate() const = 0;					// トラックを作るときのサンプリング周波数
	virtual void PlayTrack(const std::vector<float>& mono) = 0;	// トラック（メトロノーム）を頭から鳴らす
	virtual void StopTrack() = 0;
	virtual double HeardTrackSec() = 0;					// 今聞こえている所（トラックの何秒目）。まだなら -1

	// 参考：ライブラリが言う「音を出すまでの遅れ」（ms）。分からなければ -1
	virtual double ReportedLatencyMs() = 0;

protected:
	std::string m_Error;
};

std::unique_ptr<LabAudio> CreateLabXAudio2();
std::unique_ptr<LabAudio> CreateLabAdx();		// AdxMusic::Init が済んでいること
std::unique_ptr<LabAudio> CreateLabWasapi();	// 自作：WASAPI を直接使う
