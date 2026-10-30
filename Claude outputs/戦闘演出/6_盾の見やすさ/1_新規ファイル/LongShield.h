#pragma once
#include <vector>
#include <random>
#include <functional>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;

// ============================================================
// LongShield : ロングノーツを押している間、敵が前に張る盾
//   ・ロングが始まると盾が張られる（張られている間、普段の攻撃はダメージにならない）
//   ・曲がり角をうまく切り返すたび、そして最後まで押し切ったら、盾にヒビが入る
//   ・全部うまくいったら盾が割れる（OnBreak。ダメージや演出は ChartObject 側でやる）
//   ・1つでも失敗したら、その盾はもう割れない。ロングが終わると静かに消える
//   ・ロングが重なった時は、盾は1枚のまま、必要なヒビの数を足し合わせる
//
//   邪魔にならないための工夫
//     ・盾はふだん薄く、ヒビが入った瞬間だけ明るくなる
//     ・盾の下の方ほど薄くする（下はレーンに重なりやすいため）
//     ・破片はカメラの方へ飛ばさず、上と横、敵の方へ散らす
//     ・はじいた波紋は 0.1秒に1つまで
//
//   ヒビ（ガラスのような割れ方）
//     ・1回のヒビ = 1つの起点から放射状に伸びる3～4本の線 ＋ 枝分かれ ＋ 線どうしをつなぐ短い線（蜘蛛の巣）
//     ・入った瞬間に起点から伸びていき、起点がパッと光る
//     ・線は起点ほど濃く、先へ行くほど薄い
//
//   形はすべて「盾の面の上の2D座標（x = 画面の右、y = 画面の上）」で持っておき、
//   描く時に「盾の中心 ＋ 右 × x ＋ 上 × y」でワールド座標に直す
// ============================================================
class LongShield
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	using OnBreakFn = std::function<void(int longCount, const Vector3& center)>;	// 割れた（何本のロングで割ったか）
	using OnCrackFn = std::function<void(int done, int required)>;				// ヒビが入った（何本目か、全部で何本か）
	using OnFailFn = std::function<void()>;										// 失敗した（もう割れない）
	void SetOnBreak(OnBreakFn fn) { m_OnBreak = fn; }
	void SetOnCrack(OnCrackFn fn) { m_OnCrack = fn; }
	void SetOnFail(OnFailFn fn) { m_OnFail = fn; }

	void Init();
	void Reset();	// 曲の最初に呼ぶ
	void Update(float deltaTime, const Vector3& enemyCenter);
	void Draw(Camera* cam);

	// ---- ChartObject から知らせる出来事 ----
	void BeginLong(int checkpoints);	// checkpoints : このロングで入るヒビの数（曲がり角の数 + 終点の1）
	void Checkpoint(bool success);
	void EndLong();

	bool IsUp() const { return m_State == State::Rising || m_State == State::Up; }	// 張られている（ダメージを止める）
	Vector3 GetCenter() const;			// 盾の中心（ワールド座標）
	Vector3 GetRandomSurfacePos();		// 盾の表面のランダムな位置（攻撃の光が当たる所。上の方に寄せる）
	void Ripple(const Vector3& worldPos);	// 攻撃をはじいた所に波紋を出す

	// ---- 調整用：大きさ・位置 ----
	static constexpr float kRadius = 180.0f;	// 盾の大きさ（六角形の中心から角まで）
	static constexpr float kFront = 80.0f;		// 敵の中心からカメラの方へどれだけ出すか（大きいほどレーンに近づく）
	static constexpr float kLift = 90.0f;		// 敵の中心から画面の上へどれだけずらすか（大きいほどレーンから離れる）

	// ---- 調整用：濃さ（邪魔にならないように）----
	static constexpr float kBaseAlpha = 0.45f;		// ふだんの盾の濃さ（ヒビが入った瞬間だけ 1 に近づく）
	static constexpr float kFadeTop = 0.2f;			// 盾の下の方を薄くし始める高さ（半径に対する割合。上が +1、下が -1）
	static constexpr float kFadeBottom = -0.9f;		// ここで一番薄くなる
	static constexpr float kFadeMin = 0.15f;		// 一番薄い所の濃さ

	// ---- 調整用：時間 ----
	static constexpr float kRiseTime = 0.18f;	// 張られるまで
	static constexpr float kFadeTime = 0.30f;	// 割れずに消えるまで
	static constexpr float kBreakTime = 0.15f;	// 割れてから盾が見えなくなるまで（破片はもっと長く飛ぶ）
	static constexpr float kCrackGrow = 0.08f;	// ヒビが伸び切るまで
	static constexpr float kRippleInterval = 0.1f;	// 波紋を出す最短の間隔

	// ---- 調整用：破片 ----
	static constexpr int   kShardCount = 18;		// 割れた時の破片の数
	static constexpr float kShardGravity = 350.0f;	// 破片が落ちる強さ（弱いほどレーンへ落ちてこない）

private:
	enum class State { Down, Rising, Up, Breaking, Fading };

	struct P2 { float x = 0.0f, y = 0.0f; };	// 盾の面の上の2D座標
	struct Crack
	{
		std::vector<std::vector<P2>> lines;	// ヒビの線（起点から伸びる腕・枝・つなぎ）
		P2 origin;							// 起点
		float age = 0.0f;
	};
	struct Shard
	{
		float x, y, z;		// 盾の面の上の位置（z はカメラの方への飛び出し。マイナスで敵の方）
		float vx, vy, vz;	// 速さ
		float rot, spin;	// 回転（ラジアン）と回る速さ
		float size;
		float age, life;
	};
	struct RippleFx { P2 pos; float size = 30.0f; float life = 0.3f; float age = 0.0f; Vector3 color; };

	void Break();
	void AddCrack();
	void AddCrackLine(Crack& c, P2 start, float dir, float len, int depth);	// ギザギザの線を1本（たまに枝分かれ）
	void SpawnShards();
	void Basis(Vector3& right, Vector3& up, Vector3& toCam) const;
	Vector3 ToWorld(float x, float y, float z = 0.0f) const;	// 盾の面の上の座標 → ワールド座標
	float VerticalFade(float y) const;	// 盾の下の方ほど薄くする割合（0～1）
	void Flush(Camera* cam, float width, float intensity, const Vector3& glow);
	void AddHex(float cx, float cy, float radius, float rot, const Vector3& color, float alpha);	// 六角形の点を足す
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;

	State m_State = State::Down;
	float m_StateTime = 0.0f;	// 今の状態になってからの秒数
	int   m_Active = 0;			// 今押されている（進行中の）ロングの数
	int   m_Longs = 0;			// この盾に関わったロングの数
	int   m_Required = 0;		// 割るのに必要なヒビの数
	int   m_Done = 0;			// 入ったヒビの数
	bool  m_Failed = false;		// 失敗したか（もう割れない）
	float m_FailFlash = 0.0f;	// 失敗した瞬間 1 → 0
	float m_HitPulse = 0.0f;	// ヒビが入った瞬間 1 → 0
	float m_LastRipple = -100.0f;

	std::vector<Crack> m_Cracks;
	std::vector<Shard> m_Shards;
	std::vector<RippleFx> m_Ripples;
	float m_LastCrackAngle = 0.0f;

	Vector3 m_Center;
	Vector3 m_CamPos;
	bool  m_HasCam = false;
	float m_Time = 0.0f;

	std::mt19937 m_Rng{ 9753u };
	OnBreakFn m_OnBreak;
	OnCrackFn m_OnCrack;
	OnFailFn  m_OnFail;
};
