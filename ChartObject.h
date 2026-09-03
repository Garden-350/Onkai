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

	// ── 判定の進行状態 ──
	bool judged = false;			// true になったら以後描画・判定の対象から外す（Update末尾で一括削除される）
	bool approcachPlayed = false;	// 接近時の効果音を一度だけ鳴らすためのフラグ

	// ── LONGNOTE専用の状態 ──
	int m_LongTick = 0;
	bool  longStarted = false;	// 判定ラインに先端が到達し、押し始めが成立したか
	bool  longFailed = false;	// 押しっぱなしを離してしまい失敗が確定したか
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

// ============================================================
// ホーミングビーム1発分のデータと実行時状態
// ============================================================
struct ChartHomingBeam
{
	float chargeTime = 0.0f;	// 探索(タンタンタン)を開始する時刻
	float lockTime = 0.0f;		// 発射する時刻
	float hitTime = 0.0f;		// 命中判定(ダメージ/ドッジ確定)を打ち切る時刻

	// ビームの進行段階
	enum class State
	{
		Homing,		// 探索中〜発射待ち。プレイヤーを追いかけながらバラけた線を表示
		Fired,		// 発射済み。命中/回避を一定間隔でチェック中
		Resolved	// 判定終了。以後何もしない
	};
	State state = State::Homing;

	bool judgmentLocked = false;			// ロック位置が確定したか
	int  lockedSide = 0;					// ロック時のプレイヤー位置(-1/0/1)
	DirectX::SimpleMath::Vector3 lockedPos{};	// ロックされた着弾座標

	bool lockSoundPlayed = false;	// ロック音を鳴らしたか
	bool fireSoundPlayed = false;	// 発射音を鳴らしたか
	int  searchBeatsPlayed = 0;	// 「タン」を何回鳴らしたか
	float nextCountTime = 0.0f;	// 次に命中判定を行う時刻

	static constexpr int   kSearchBeatCount = 3;	// 「タン」を鳴らす回数
	static constexpr float kCountInterval = 0.5f;	// 発射後、何秒おきに命中判定するか
	static constexpr int   kScatterCount = 6;		// 探索中に表示するバラけた線の本数
	DirectX::SimpleMath::Vector3 scatterOffsets[kScatterCount]; // 各線のランダムな初期オフセット
};

struct BeamShaderParms
{
	float time;
	float intensity;
	float padding0[2];
	DirectX::SimpleMath::Vector4 baseColor;
};	//sizeof == 32 (16の倍数ならOK)

struct BarrierEffect
{
	bool active = false;
	float timer = 0.0f;
};

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
	int GetPerfectCount() const { return m_Perfect; }
	int GetGreatCount()   const { return m_Great; }
	int GetGoodCount()    const { return m_Good; }
	int GetMissCount()    const { return m_Miss; }
	int GetMaxComboCount() const { return m_MaxCombo; }

	float GetFlashAlpha()
	{
		return (m_FlashDuration > 0.0001f) ? (m_FlashTimer / m_FlashDuration) : 0.0f;
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
	Sound sound;
	Shader m_Shader;
	MeshRenderer m_MeshRenderer;
	MagicCircleQuad m_MagicCircleQuad;
	RingQuad m_ApproachRing;		// 接近リング（判定ラインに近づくほど大きく濃くなる）
	ParticleQuad m_ParticleQuad;	// 判定ヒット時の花びらパーティクル
	BarrierQuad m_BarrierQuad;
	Texture2D m_LaneIcons[6];		// S/D/F/J/K/Lアイコン（画面固定2D）
	Texture2D m_ComboDigits[4];	// コンボ数の3桁表示
	Texture2D m_PerfectDigits[4];
	Texture2D m_GreatDigits[4];
	Texture2D m_GoodDigits[4];
	Texture2D m_MissDigits[4];
	Texture2D m_LaneFrame[6];		// 判定円の外枠
	Texture2D m_LaneFill[6];		// 判定円の内側（接近度に応じて育つ）
	BarrierEffect m_BarrierEffects[6];
	std::vector<SUBSET> m_Subsets;
	std::vector<Particle> m_Particle;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<ChartKeyLightEffect> m_LightEffects;	// 判定時のキーライト点滅エフェクト
	std::chrono::steady_clock::time_point m_LastUpdateClock;
	std::string m_AudioFile;
	float m_NoteScale = 40.0f;	// TAPモデルの表示スケール

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

	// ────────────────────────────────────────
	// ホーミングビーム用モデル（棒状FBXを引き伸ばして使う想定）
	// ────────────────────────────────────────
	bool m_BeamModelLoaded = false;
	MeshRenderer m_BeamMeshRenderer;
	Texture m_BeamNoiseTexture;
	Shader m_BeamShader;
	std::vector<SUBSET> m_BeamSubsets;
	std::vector<std::unique_ptr<Material>> m_BeamMaterials;
	std::vector<std::unique_ptr<Texture>> m_BeamTextures;
	float m_BeamModelLength = 1.0f;	// モデルのローカルZ方向の長さ（アセットの実寸に合わせて調整）
	DirectX::SimpleMath::Vector3 GetPlayerSideAnchor(int side)const;
	DirectX::SimpleMath::Vector3 m_ShakeOffset = DirectX::SimpleMath::Vector3::Zero;

	float m_ShakeTimer = 0.0f;
	float m_ShakeDuration = 0.0f;
	float m_ShakeStrength = 0.0f;
	float m_FlashTimer = 0.0f;
	float m_FlashDuration = 0.0f;
	float m_BeamShaderTime = 0.0f;
	
	void TriggerScreenShake(float duration, float strength);
	void TriggerFlash(float duration);

	bool m_AutoMode = false;
	void UpdateAutoJudgge();	//autoモード


	// ────────────────────────────────────────
	// 譜面データ本体
	// ────────────────────────────────────────
	std::vector<ChartNote> m_Notes;
	std::vector<ChartJudgeCircle> m_JudgeCircles;	// レーン0〜5に対応
	std::vector<ChartHomingBeam> m_HomingBeams;

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
	int m_Combo = 0;		// 現在のコンボ数

	// ── 判定カウント ──
	int m_Perfect = 0;
	int m_Great = 0;
	int m_Good = 0;
	int m_Miss = 0;
	int m_MaxCombo = 0;

	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
	float m_FrameSize = 150.0f;			// 判定円の外枠の表示サイズ

	// ── 判定タイミングの許容誤差（秒）。値が小さいほどシビア ──
	static constexpr float kPerfect = 0.2f;
	static constexpr float kGreat = 0.4f;
	static constexpr float kGud = 0.6f;			// この範囲を過ぎたら自動ミス
	static constexpr float kLongTickInterval = 0.05f;	// ロングノーツの継続判定(tick)間隔

	bool m_PrevLaneKeyDown[6] = { false,false,false,false,false,false }; // （現状未使用。将来の押した瞬間検出用に予約）

	// ── ノーツの位置補間 ──
	// TAP/FLICK用：spawnPos〜judgePosを [0,1] にクランプして補間（ベジェ対応）
	DirectX::SimpleMath::Vector3 EvaluateNotePosition(const ChartNote& note, float now) const;
	// LONGNOTE用：spawnPos〜judgePosを「クランプせず」直線補間（先端が判定ラインを越えても進み続けられるように）
	DirectX::SimpleMath::Vector3 EvaluateLongNotePosition(const ChartNote& note, float now) const;

	// ── 判定処理 ──
	void JudgeLane(int key, int laneIndex);	// TAPノーツ：キーを押した瞬間の判定
	void JudgeFlickNotes();					// FLICKノーツ：時間ウィンドウ内でサイド一致を判定
	void JudgeLongNoteHold();					// LONGNOTE：押し始め〜押しっぱなし〜離すを判定

	// ── ホーミングビーム ──
	void UpdateHomingBeams(float deltaTime);	// 状態遷移（探索→ロック→発射→解決）とプレイヤー移動更新
	void InitBeamScatter(ChartHomingBeam& beam);	// 探索中に表示するバラけた線の初期位置を決める
	void DrawBeamSegment(Camera* cam,				// from→to の間にビームモデルを引き伸ばして1本描画
		const DirectX::SimpleMath::Vector3& from,
		const DirectX::SimpleMath::Vector3& to,
		float thicknessX, float thicknessY);
	void DrawBeamSegmentWithColor(Camera* cam, const DirectX::SimpleMath::Vector3& from,
		const DirectX::SimpleMath::Vector3& to, float thicknessX, float thicknessY,
		const DirectX::SimpleMath::Vector3& baseColor, float intensity);
	void DrawJudggedBeam(Camera* cam, const DirectX::SimpleMath::Vector3& from,
		const DirectX::SimpleMath::Vector3& to, float thickness,
		const DirectX::SimpleMath::Vector3& color, float intensity,
		int segments = 6, float jitter = 15.0f);

	void DrawHomingBeams(Camera* cam);				// 全ビームの描画をまとめて行う

	// ── ロングノーツ描画（sentan/dou/oの3パーツ） ──
	void DrawLongNotes(Camera* cam);
	// ── フリックノーツ描画 ──
	void DrawFlickNotes(Camera* cam);

	// ── 判定ヒット時のフック：コンボ加算＋パーティクル生成 ──
	void OnJudge(int laneIndex, int rank); // rank: 0=PERFECT; 1=GREAT 2=GOOD
	void UpdateLightEffects(float deltaTime);
	void DrawLightEffects(Camera* cam);

	struct JudgeRankEffect
	{
		bool active = false;
		float timer = 0.0f;
		int rank = 0; // 0=PERFECT, 1=GREAT, 2=GOOD
	};

	Texture2D m_JudgeRankPerfect;
	Texture2D m_JudgeRankGreat;
	Texture2D m_JudgeRankGood;
	JudgeRankEffect m_JudgeRankEffects[6];
	static constexpr float kJudgeRankPopDuration = 0.6f; // 表示継続時間（秒）

	static DirectX::SimpleMath::Vector3 GetLaneColor(int lane)
	{
		switch (lane)
		{
		case 0: case 3: return DirectX::SimpleMath::Vector3(1.0f, 0.15f, 0.15f); // S, J = 赤
		case 1: case 4: return DirectX::SimpleMath::Vector3(0.2f, 0.4f, 1.0f);   // D, K = 青
		case 2: case 5: return DirectX::SimpleMath::Vector3(0.2f, 1.0f, 0.3f);   // F, L = 緑
		default: return DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
		}
	}

	//ボタンを押したときに出すエフェクトの一種
	static constexpr float kBarrierDuration = 10.25f;
};