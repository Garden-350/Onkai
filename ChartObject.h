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
#include "MagicCircleQuad.h"
#include "BarrierQuad.h"
#include "ChartHomingBeam.h"
#include "ScreenEffects.h"
#include "HomingBeamSystem.h"
#include "JudgeSystem.h"
#include "ComboHud.h"
#include "ClosingRingGuide.h"
#include "BarrierPlate.h"

class Player;
class Enemy;

// ============================================================
// ノーツ種別
// ※ エディター(Game.cpp)側の NoteType enum と数値を必ず一致させること！
//    TAP=0, LONGNOTE=1, FLICK_RIGHT=2, FLICK_LEFT=3
// ============================================================
enum class ChartNoteType
{
	TAP = 0,
	LONGNOTE = 1,
	FLICK_RIGHT = 2,
	FLICK_LEFT = 3
};

// ============================================================
// 1つのノーツが持つデータ
// エディターが吐き出す JSON の1要素をそのまま格納する入れ物
// ============================================================
struct ChartNote
{
	int lane = 0;			// レーン番号(0〜5 = S,D,F,J,K,L)
	int type = 0;			// ノーツ種別。ChartNoteType のint値を入れる
	float time = 0.0f;		// 終了/判定時刻（TAPなら判定時刻、LONGなら終端が判定ラインに着く時刻）
	float spawnTime = -1.0f;	// 出現時刻。-1なら GetSpawnTime() が自動計算する
	float arrivalTime = 0.0f;	// LONGNOTE専用：先端が判定ラインに到達する時刻
	float holdDuration = 0.0f;	// LONGNOTE専用：保持時間（エディタUI入力補助用。実描画では time-arrivalTime を都度計算するので必須ではない）
	int flickLaneTo = -1;		// FLICK専用：フリックの着地先レーン。-1なら「lane と同じ」扱い

	DirectX::SimpleMath::Vector3 spawnPos{};	// 出現位置（ワールド座標）
	DirectX::SimpleMath::Vector3 judgePos{};	// 判定位置（ワールド座標）
	std::vector<DirectX::SimpleMath::Vector3> controlPoints;	// ベジェ曲線の制御点。0個=直線 1個=2次ベジェ 2個=3次ベジェ

	// ── フィナーレ演出用：ロングノーツを螺旋状に描画するオプション（無指定なら従来通り直線）──
	float spiralTurns = 0.0f;		// 螺旋の回転数（例:2.0で2回転）。0なら螺旋なし
	float spiralRadius = 0.0f;		// 螺旋の半径(spawn時が最大、判定時刻に向けて0へ収束)

	// ── 判定の進行状態 ──
	bool judged = false;			// true になったら以後描画・判定の対象から外す（Update末尾で一括削除される）
	bool approcachPlayed = false;	// 接近時の効果音を一度だけ鳴らすためのフラグ

	// ── LONGNOTE専用の状態 ──
	float nextTickTime = 0.0f;	// 次に「継続判定(tick)」を加算すべき時刻

	// 出現時刻を取得。未設定なら「判定時刻の1秒前」をデフォルトとする
	float GetSpawnTime() const
	{
		if (spawnTime >= 0.0f) return spawnTime;
		return time - 1.0f;
	}

	// ── 種別判定ヘルパー（type番号を直接比較しないためのラッパー）──
	bool IsTap()    const { return type == (int)ChartNoteType::TAP; }
	bool IsLong()   const { return type == (int)ChartNoteType::LONGNOTE; }
	bool IsFlick()  const { return type == (int)ChartNoteType::FLICK_RIGHT || type == (int)ChartNoteType::FLICK_LEFT; }
	bool IsFlickRight() const { return type == (int)ChartNoteType::FLICK_RIGHT; }
};

// ============================================================
// 判定円の1キーフレーム（time秒時点でのxyz座標）
// ============================================================
struct ChartCircleKeyframe
{
	float time = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

// ============================================================
// 判定円1個分のアニメーションデータ（レーン0〜5、各1個）
// ============================================================
struct ChartJudgeCircle
{
	int lane = 0;
	std::vector<ChartCircleKeyframe> keyframe;

	// now(秒)時点の位置を、前後のキーフレーム間で線形補間して返す
	DirectX::SimpleMath::Vector3 GetPosition(float now) const;
};

// ChartHomingBeam / BeamShaderParms は ChartHomingBeam.h に切り出し済み

struct ChartKeyLightEffect
{
	int lane = 0;
	float startTime = 0.0f;
	float duration = 0.0f;
	float intensity = 2.2f;	// 1.0f=通常 2.0f=2倍明るい 3.0f=3倍明るい
	DirectX::SimpleMath::Vector3 origin;
};

// ============================================================
// ChartObject : エディターが吐き出した .json 譜面を読み込んで
//               実際にノーツ/ビームを流し、判定するプレイ専用オブジェクト。
//
// 対応ノーツ：TAP / LONGNOTE(sentan・dou・oの3パーツ) / FLICK(左右) / ホーミングビーム
// ============================================================
class ChartObject : public Object
{
public:

	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	// Stage1Scene::Init() から呼ぶ：charts/<name>.json を読み込んで再生準備する
	bool LoadChart(const std::string& name);

	// ビームの発射元座標として Enemy を紐付ける（Stage1Sceneから呼ぶ）
	void SetEnemyRef(Enemy* enemy) { m_EnemyRef = enemy; }

	void SetPlayerRef(Player* player) { m_PlayerRef = player; }

	// 判定結果の参照用
	int GetPerfectCount() const { return m_JudgeSystem.GetPerfectCount(); }
	int GetGreatCount()   const { return m_JudgeSystem.GetGreatCount(); }
	int GetGoodCount()    const { return m_JudgeSystem.GetGoodCount(); }
	int GetMissCount()    const { return m_JudgeSystem.GetMissCount(); }
	int GetMaxComboCount() const { return m_JudgeSystem.GetMaxComboCount(); }
	int GetTotalNoteCount() const { return m_TotalNote; }
	// フルコンボ判定：ミスなく最後まで叩けていれば MaxCombo == 総ノーツ数 になる
	bool IsFullCombo() const { return m_TotalNote > 0 && m_JudgeSystem.GetMaxComboCount() == m_TotalNote; }
	float GetFlashAlpha()
	{
		return m_ScreenEffects.GetFlashAlpha();
	}
	//曲が最後まで再生したか
	bool IsAudioFinish() const
	{
		if (m_AudioFile.empty()) return false;
		float duration = sound.GetDynamicDurationSeconds();
		if (duration <= 0.0f) return false;
		return m_MusicTime >= duration;
	}

	void DrawLaneGuideLines(Camera* cam);

private:
	// ────────────────────────────────────────
	// TAPノーツ本体の3Dモデル・エフェクト一式
	// ────────────────────────────────────────

	ClosingRingGuide m_ClosingRingGuide;	// 判定円の外枠が閉じるやつ
	BarrierPlate m_BarrierPlate;
	JudgeSystem m_JudgeSystem;	// 判定結果の集計・参照用
	Sound sound;
	Shader m_Shader;
	MeshRenderer m_MeshRenderer;
	MagicCircleQuad m_MagicCircleQuad;
	RingQuad m_ApproachRing;		// 接近リング（判定ラインに近づくほど大きく濃くなる）
	ParticleQuad m_ParticleQuad;	// 判定ヒット時の花びらパーティクル
	Texture2D m_LaneIcons[6];		// S/D/F/J/K/Lアイコン（画面固定2D）
	bool m_ShowLaneKeyIcons = false;	// レーンキーアイコン(S/D/F/J/K/L)を表示するか。視認性改善のためデフォルトOFF
	Texture2D m_LaneFrame[6];		// 判定円の外枠
	bool m_ShowLaneFrame = false;		// 判定円の外枠(sotohako)を表示するか。デフォルトOFF
	Texture2D m_LaneFill[6];		// 判定円の内側（接近度に応じて育つ・フェードイン）
	std::vector<SUBSET> m_Subsets;
	std::vector<Particle> m_Particle;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<ChartKeyLightEffect> m_LightEffects;	// 判定時のキーライト点滅エフェクト
	std::chrono::steady_clock::time_point m_LastUpdateClock;
	std::string m_AudioFile;
	float m_NoteScale = 40.0f;	// TAPモデルの表示スケール
	float m_BarrierSparkleTimer[6] = { 0,0,0,0,0,0 };	//レーンごとのキラキラパーティクル生成タイマー

	void DrawSpawnMagicCircle(Camera* cam, const ChartNote& note);

	// ────────────────────────────────────────
	// ロングノーツ用モデル（sentan=先端／dou=本体(鎖)／o=終端の輪）
	// エディター(Game.cpp)の m_Long_Sentan_Model 等に対応する3パーツ構成。
	// ロード失敗時は個別に「TAPモデルで代用」にフォールバックする。
	// ────────────────────────────────────────
	bool m_LongSentanLoaded = false;
	MeshRenderer m_LongSentanRenderer;
	Shader m_LongSentanShader;
	std::vector<SUBSET> m_LongSentanSubsets;
	std::vector<std::unique_ptr<Material>> m_LongSentanMaterials;
	std::vector<std::unique_ptr<Texture>> m_LongSentanTextures;

	bool m_LongDouLoaded = false;
	MeshRenderer m_LongDouRenderer;
	Shader m_LongDouShader;
	std::vector<SUBSET> m_LongDouSubsets;
	std::vector<std::unique_ptr<Material>> m_LongDouMaterials;
	std::vector<std::unique_ptr<Texture>> m_LongDouTextures;

	bool m_LongOLoaded = false;
	MeshRenderer m_LongORenderer;
	Shader m_LongOShader;
	std::vector<SUBSET> m_LongOSubsets;
	std::vector<std::unique_ptr<Material>> m_LongOMaterials;
	std::vector<std::unique_ptr<Texture>> m_LongOTextures;

	// ────────────────────────────────────────
	// フリック用モデル（無ければTAPモデルで代用）
	// ────────────────────────────────────────
	bool m_FlickModelLoaded = false;
	MeshRenderer m_FlickMeshRenderer;
	Shader m_FlickShader;
	std::vector<SUBSET> m_FlickSubsets;
	std::vector<std::unique_ptr<Material>> m_FlickMaterials;
	std::vector<std::unique_ptr<Texture>> m_FlickTextures;

	DirectX::SimpleMath::Vector3 GetPlayerSideAnchor(int side)const;
	ScreenEffects m_ScreenEffects;
	HomingBeamSystem m_BeamSystem;

	bool m_AutoMode = false;


	// ────────────────────────────────────────
	// 譜面データ本体
	// ────────────────────────────────────────
	std::vector<ChartNote> m_Notes;
	std::vector<ChartJudgeCircle> m_JudgeCircles;	// レーン0〜5に対応

	Enemy* m_EnemyRef = nullptr;	// ビームの発射元座標を取得するための参照

	// ── プレイヤーの左右移動（S/D/F=右, J/K/L=左）──
	Player* m_PlayerRef = nullptr;
	//int   m_PlayerSide = 0;			// 現在の入力サイド(-1/0/1)
	//float m_PlayerVisualX = 0.0f;		// 見た目上のX座標（補間で追従させる）
	//DirectX::SimpleMath::Vector3 m_PlayerBasePos{ 0.0f, -55.0f, -260.0f }; // プレイヤーの基準座標。実際の見た目に合わせて要調整
	//float m_PlayerMoveRange = 100.0f;	// 左右移動時のX方向の振れ幅。要調整
	//float m_PlayerMoveSpeed = 800.0f;	// 移動の補間速度。要調整

	// ── 再生時間管理（BGM同期はスコープ外。経過時間ベースで進める）──
	std::chrono::steady_clock::time_point m_StartClock;
	bool m_Started = false;
	bool m_AudioStarted = false;	//音楽再生をスタートしたか
	float m_MusicTime = 0.0f;		// 現在の楽曲内時刻（カウントイン中は負の値）
	float m_Bpm = 120.0f;			// メタデータから読み込むBPM
	float m_CountInTime = 0.0f;	// カウントインの長さ（1小節分の秒数）
	int m_CountInNextBeat = 0;		// 次に鳴らすべきカウントインの拍番号

	int m_BeatsPerBar = 4;	// 拍子（メタデータから読み込み）
	float m_FrameSize = 150.0f;			// 判定円の外枠の表示サイズ
	int m_TotalNote = 0;	//譜面のノーツ数
	bool m_FullCombo = false;	//フルコンボ達成したかどうか

	bool m_PrevLaneKeyDown[6] = { false,false,false,false,false,false };

	// ── ロングノーツ描画（sentan/dou/oの3パーツ） ──
	void DrawLongNotes(Camera* cam);
	// ── フリックノーツ描画 ──
	void DrawFlickNotes(Camera* cam);

	// ── 判定ヒット時のフック：コンボ加算＋パーティクル生成 ──
	void OnJudge(int laneIndex, int rank); // rank: 0=PERFECT; 1=GREAT 2=GOOD
	void OnEmptyLanePress(int laneIndex);  // 空押し時のライトエフェクト（元JudgeLane内の処理）
	void UpdateLightEffects(float deltaTime);
	void DrawLightEffects(Camera* cam);

	static DirectX::SimpleMath::Vector3 GetLaneColor(int lane)
	{
		switch (lane)
		{
		case 0: case 3: return DirectX::SimpleMath::Vector3(1.0f, 0.15f, 0.15f); // S, J = 赤
		case 1: case 4: return DirectX::SimpleMath::Vector3(0.2f, 1.0f, 0.3f);   // F, L = 緑
		case 2: case 5: return DirectX::SimpleMath::Vector3(0.2f, 0.4f, 1.0f);  // D, K = 青
		default: return DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
		}
	}
	ComboHud m_ComboHud;
};