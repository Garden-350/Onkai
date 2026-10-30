#pragma once
#include <vector>
#include <random>
#include <functional>
#include <SimpleMath.h>
#include "LongBeamRibbon.h"

class Camera;
class LongShield
{
public:
	using Vector3 = DirectX::SimpleMath::Vector3;
	using OnBreakFn = std::function<void(int longCount, const Vector3& center)>;	//何本目のロングで割ったか
	using OnCrackFn = std::function<void(int done, int required)>;	//ヒビが入った。何本か
	using OnFailFn = std::function<void()>;	//失敗した

	void SetOnBreak(OnBreakFn fn) { m_OnBreak = fn; }
	void SetOnCrack(OnCrackFn fn) { m_OnCrack = fn; }
	void SetOnFail(OnFailFn fn) { m_OnFail = fn; }

	void Init();
	void Reset();	//曲の最初に呼ぶ
	void Update(float deltaTime, const Vector3& enemyCenter);
	void Draw(Camera* cam);

	void BeginLong(int checkpoints);	//checkpoints:ロングで入るヒビの数（曲がり角の数＋終点の1）
	void Checkpoint(bool success);
	void EndLong();

	bool IsUp() const { return m_State == State::Rising || m_State == State::Up; }	//張られている（ダメージを止める）
	Vector3 GetCenter() const;	//盾の中心（ワールド座標）
	Vector3 GetRandomSurfacePos();	//盾の表面のランダムな位置（攻撃の光が当たるところ）
	void Ripple(const Vector3& worldPos);	//攻撃をはじいたところに波紋を出す

	//調整用
	static constexpr float kRadius = 160.0f;	//盾の大きさ
	static constexpr float kFront = 0.0f;
	static constexpr float kLift = 30.0f;	//敵の中心から画面の上へどれだけずらすか
	static constexpr float kStretchx = 1.3f;	//横幅 縦を短くしたい場合

	//調整用：色
	static constexpr float kBaseAlpha = 0.45f;	//ふだんの盾の濃さ
	static constexpr float kFadeTop = 0.2f;		//盾の下の方を薄くする
	static constexpr float kFadeBottom = -0.9f;	//一番薄いところの位置
	static constexpr float kFadeMin = 0.15f;	//一番薄いところの濃さ

	//調整用：時間
	static constexpr float kRiseTime = 0.18f;	//張られるまでの時間
	static constexpr float kFadeTime = 0.3f;		//割れずに消えるまで
	static constexpr float kBreakTime = 0.15f;	//割れてから盾が見えなくなるまで
	static constexpr float kCrackGrow = 0.08f;	//ヒビが伸びきるまでの時間
	static constexpr float kRippleInterval = 0.1f;	//波紋を出す間隔

	//調整用：見た目
	static constexpr int kShardCount = 18;	//割れた時の破片の数
	static constexpr int kCrackSegments = 6;//ヒビ一本のぎざぎざの数
	static constexpr float kShardGravity = 350.0f;

private:
	enum class State
	{
		Down,
		Rising,
		Up,
		Breaking,
		Fading
	};

	struct P2 { float x = 0.0f, y = 0.0f; };	//盾の上の2D座標
	struct Crack 
	{ 
		std::vector<P2> pts;	//ヒビの線 起点から伸びる
		std::vector<std::vector<P2>> lines;
		P2 origin;	//起点
		float age = 0.0f;
	};
	struct Shard
	{
		float x, y, z;	//盾の面の上の位置（ｚはカメラの方への飛び出し）
		float vx, vy, vz;	//速さ
		float rot, spin;	//回転
		float size;
		float age, life;
	};
	struct RippleFx 
	{
		P2 pos;
		float size = 30.0f;
		float life = 0.3f;
		float age = 0.0f;
		Vector3 color;
	};

	void Break();
	void AddCrack();
	void AddCrackLine(Crack& c, P2 start, float dir, float len, int depth);	//ぎざぎざの線を一本
	void SpawnShards();
	void Basis(Vector3& right, Vector3& up, Vector3& toCam) const;
	Vector3 ToWorld(float x, float y, float z = 0.0f) const;	//盾の面の上の座標->ワールド座標
	void Flush(Camera* cam, float width, float intensity, const Vector3& glow);
	void AddHex(float cx, float cy, float radius, float rot, const Vector3& color, float alpha);	//六角形の点を足す
	float Rand(float a, float b);
	float VerticalFade(float y) const;	//盾の下の方を薄くする割合

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;

	State m_State = State::Down;
	float m_StateTime = 0.0f;	//今の状態になってからの秒数
	int m_Active = 0;			//今押されているロングの本数
	int m_Longs = 0;			//この盾に関わったロングの本数
	int m_Required = 0;			//割るのに必要なヒビの数
	int m_Done = 0;				//入ったヒビの数
	bool m_Failed = false;		//失敗したか
	float m_FailFlash = 0.0f;	//失敗した瞬間1->0
	float m_HitPulse = 0.0f;	//ヒビが入った瞬間1->0
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

