#pragma once
#include "Object.h"
#include "StaticMesh.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Texture.h"
#include "RingQuad.h"
#include "Texture2D.h"
#include "ParticleQuad.h"
#include "sound.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "Particle.h"

// エディターで作った譜面(JSON)を読み込み、ノーツを3Dモデルで流して
// S/D/F/J/K/Lキーで判定するだけの再生専用オブジェクト。
struct ChartNote
{
	int lane = 0;
	int type = 0;	//今はTAPしかない
	float time = 0.0f;	//判定時刻
	float spawnTime = -1.0f;	//出発時刻（未設定なら time - kDefaultTravel で自動計算）
	DirectX::SimpleMath::Vector3 spawnPos{};
	DirectX::SimpleMath::Vector3 judgePos{};
	std::vector<DirectX::SimpleMath::Vector3> controlPoints;	// 0=直線 1=2次ベジェ 2=3次ベジェ

	bool judged = false; // 判定済み/ミス確定済みなら true（以後描画しない）
	bool approcachPlayed = false;	//判定前に一度だけ鳴らす音フラグ

	float GetSpawnTime() const
	{
		if (spawnTime >= 0.0f) return spawnTime;
		return time - 1.0f;	// デフォルト：判定1秒前に出現
	}
};

//判定円のキーフレームアニメーション
struct ChartCircleKeyframe
{
	float time = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct ChartJudgeCircle
{
	int lane = 0;
	std::vector<ChartCircleKeyframe> keyframe;

	//now(秒)における位置をキーフレーム間の線形補完で求める
	DirectX::SimpleMath::Vector3 GetPosition(float now) const;
};

class ChartObject : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	// Stage1Scene::Init() から呼ぶ：JSON譜面をロードして再生準備をする
	// name : "charts/<name>.json" というパスのファイル名部分（拡張子・ディレクトリ不要）
	bool LoadChart(const std::string& name);

	//判定結果
	int GetPerfectCount() const { return m_Perfect; }
	int GetGreatCount()   const { return m_Great; }
	int GetGoodCount()    const { return m_Good; }
	int GetMissCount()    const { return m_Miss; }

private:
	//ノーツ本体の3Dモデル一式
	Sound sound;
	MeshRenderer m_MeshRenderer;
	RingQuad m_ApproachRing;	// 接近リング（3D空間）
	ParticleQuad m_ParticleQuad;
	Texture2D m_LaneIcons[6];	// S/D/F/J/K/Lの判定円アイコン（画面固定2D）
	Texture2D m_ComboDigits[3];
	Texture2D m_LaneFrame[6];	//外側の四角
	Texture2D m_LaneFill[6];	//内側の育つ四角
	std::vector<SUBSET> m_Subsets;
	std::vector<Particle> m_Particle;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::chrono::steady_clock::time_point m_LastUpdateClock;
	float m_NoteScale = 15.0f;	//モデルの大きさ

	//譜面データ
	std::vector<ChartNote> m_Notes;
	std::vector<ChartJudgeCircle> m_JudgeCircles; // レーン0〜5に対応

	// 再生時間管理（BGM同期は今回スコープ外、経過時間だけで進める）
	std::chrono::steady_clock::time_point m_StartClock;
	bool m_Started = false;
	float m_MusicTime = 0.0f;
	float m_Bpm = 120.0f;	//BPMメタデータから読み込み
	float m_CountInTime = 0.0f;   // カウントインの長さ（1小節分の秒数）
	int m_CountInNextBeat = 0;    // 次に鳴らすべき拍番号（0始まり）

	int m_BeatsPerBar = 4;	//拍子メタデータから読み込み
	int m_Combo = 0;
	//判定
	int m_Perfect = 0;
	int m_Great = 0;
	int m_Good = 0;
	int m_Miss = 0;

	float m_ComboPopTimer = 0.0f;
	float m_NuberSplite_Size = 80.0f;

	//判定値
	static constexpr float kPerfect = 0.02f;
	static constexpr float kGreat = 0.04f;
	static constexpr float kGud = 0.06f;

	DirectX::SimpleMath::Vector3 EvaluateNotePosition(const ChartNote& note, float now) const;
	void JudgeLane(int key, int laneIndex);

	// ── 判定エフェクトのフック（後で差し替え予定）──────────────
	//   ここに回転する四角バーストの描画を実装する。
	void OnJudge(int laneIndex, int rank); // rank: 0=PERFECT 1=GREAT 2=GOOD
};

