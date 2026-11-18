#pragma once
// ============================================================
// AdxMusic : 曲（BGM）を CRI ADX LE で鳴らす
//   ・いちばん大事なのは GetTime()。「実際にスピーカーから出た音」に合わせた再生位置（秒）が取れる
//     → 譜面の時刻をこれに合わせれば、フレームの時計と音が少しずつずれていく「音ズレ」が起きない
//   ・読めるのは WAV / HCA / ADX（MP3 は読めない。WAV に変えて使う）
//   ・効果音や、曲の波形・BPM の解析は今まで通り Sound（XAudio2）で行う
//
// 使い方
//   AdxMusic::Init();                    // 起動したときに 1 回
//   AdxMusic::Update();                  // 毎フレーム 1 回
//   AdxMusic::Load("asset/bgm/a.wav");   // 曲を決める（すぐには読まない）
//   AdxMusic::Play(12.5f);               // 12.5 秒から鳴らす
//   float t = AdxMusic::GetTime();       // 今の再生位置（秒）。まだ鳴っていなければ -1
//   AdxMusic::Finalize();                // 終わるときに 1 回
// ============================================================
#include <string>

namespace AdxMusic
{
	bool Init();							// ライブラリの準備（失敗したら false。そのときは何もしない）
	void Finalize();						// 後片付け
	void Update();							// 毎フレーム呼ぶ（ADX の中の処理を進める）

	bool Load(const std::string& path);		// 鳴らす曲を決める（.wav / .hca / .adx。それ以外は false）
	void Unload();							// 曲なしにする
	bool IsLoaded();						// ADX で鳴らせる曲が決まっているか

	void Play(float fromSec);				// fromSec 秒から鳴らす（鳴っていたら止めてから）
	void Stop();
	void Pause(bool on);					// true で一時停止、false で再開
	bool IsPlaying();						// 鳴っている（または鳴らす準備中）か

	float GetTime();						// 音に合わせた再生位置（秒）。鳴っていない・準備中は -1
	void SetVolume(float volume);			// 0〜1
}
