#pragma once
#include "Object.h"
#include "LongBeamRibbon.h"
#include <vector>
#include <random>
#include <chrono>

//’@‚¢‚½‚Æ‚«‚ÌaŒ‚‚Æ”j•Ğ@GameEvent::OnJudge‚©‚ço‚·
class HitSlashFx : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }	//”»’è‰~‚Æ“¯‚¶ƒJƒƒ‰‚Å•`‚­

private:
	using Vector3 = DirectX::SimpleMath::Vector3;

	struct Slash	//aŒ‚ˆê–{
	{
		Vector3 pos;	//^‚ñ’†
		Vector3 color;
		float angle;	//‰æ–Ê‚Ìã‚Å‚ÌŒX‚«iƒ‰ƒWƒAƒ“j
		float length;	//’·‚³
		float width;	//‘¾‚³
		float age;
		float life;
	};
	struct Shard	//”j•Ğ‚ğˆê‚Â
	{
		Vector3 pos, vel;
		Vector3 color;
		float width;
		float age;
		float life;
	};

	void SpawnSlash(const Vector3& pos, const Vector3& color, float angle, float length, float width, float life);
	void SpawnShards(const Vector3& pos, const Vector3& color, int count, float speedMin, float speedMax);
	float Rand(float a, float b);

	LongBeamRibbon m_Ribbon;
	std::vector<LongBeamRibbon::Point> m_Pts;
	std::vector<Slash> m_Slashes;
	std::vector<Shard> m_Shards;
	std::mt19937 m_Rng{ 2486u };

	int m_JudgeSub = 0;	//’@‚¢‚½‚ğ•·‚¢‚Ä‚¢‚é“o˜^”Ô†
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//’²®—p
	static constexpr float kLift = 40.0f;	//”»’è‰~‚©‚ç‚Ç‚ê‚¾‚¯ã‚Åa‚é‚©
	static constexpr float kSlashLife = 0.22f;	//aŒ‚‚ªÁ‚¦‚é‚Ü‚Å‚Ì•b”
	static constexpr float kSweepTime = 0.06f;	//n‚ª’[‚©‚ç’[‚Ü‚Å‘–‚é•b”
	static constexpr float kBow = 0.18f;		//aŒ‚‚Ì”½‚èi’·‚³‚É‘Î‚·‚éŠ„‡j
	static constexpr int kMaxSlashes = 32;		//Å‘åaŒ‚”

	static constexpr float kGravity = 700.0f;	//”j•Ğ‚ª—‚¿‚é‹­‚³
	static constexpr float kDrag = 3.0f;		//”j•Ğ‚ÌŒ¸‘¬
	static constexpr int kMaxShards = 200;		//Å‘å”j•Ğ”

};

