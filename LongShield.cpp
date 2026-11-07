#include "LongShield.h"
#include "RibbonShapes.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	const Vector3 kShieldColor(0.45f, 0.75f, 1.0f);	//盾の色
	const Vector3 kFailColor(0.55f, 0.55f, 0.65f);	//失敗した盾の色
	const Vector3 kCrackColor(1.0f, 1.0f, 1.0f);	//ヒビの色
	constexpr float kTwoPi = 2.0f * RibbonShapes::kPi;
}

void LongShield::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(64);
	m_Rng.seed(std::random_device{}());
	Reset();
}

void LongShield::Reset()
{
	m_State = State::Down;
	m_StateTime = 0.0f;
	m_Active = 0;
	m_Longs = 0;
	m_Required = 0;
	m_Done = 0;
	m_Failed = false;
	m_FailFlash = 0.0f;
	m_HitPulse = 0.0f;
	m_LastRipple = -100.0f;
	m_Cracks.clear();
	m_Shards.clear();
	m_Ripples.clear();
}

float LongShield::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

//判定してるところ======================================================================================
void LongShield::BeginLong(int checkpoints)
{
	//盾がない　消えかけ・割れた直後も含む　新しい盾を張る
	if (!IsUp())
	{
		m_State = State::Rising;
		m_StateTime = 0.0f;
		m_Active = 0;
		m_Longs = 0;
		m_Required = 0;
		m_Done = 0;
		m_Failed = false;
		m_Cracks.clear();
		m_Ripples.clear();
		m_LastCrackAngle = Rand(0.0f, kTwoPi);
	}
	//盾が張られているときに始まったロングは同じ盾に足し合わせる
	m_Active++;
	m_Longs++;
	m_Required += std::max(1, checkpoints);
}

void LongShield::Checkpoint(bool success)
{
	if (!IsUp()) return;
	if (!success)
	{
		if (!m_Failed)
		{
			m_Failed = true;	//この盾はもう割れない
			m_FailFlash = 1.0f;
			if (m_OnFail) m_OnFail();
		}
		return;
	}
	if (m_Failed) return;	//失敗した後はひびを入れない

	m_Done++;
	AddCrack();
	m_HitPulse = 1.0f;
	if (m_OnCrack) m_OnCrack(m_Done, m_Required);

	if (m_Done >= m_Required) Break();	//パフェ
}

void LongShield::EndLong()
{
	m_Active = std::max(0, m_Active - 1);
	if (m_Active == 0 && IsUp())
	{
		//割れないままロングが終わったら消滅
		m_State = State::Fading;
		m_StateTime = 0.0f;
	}
}

void LongShield::Break()
{
	m_State = State::Breaking;
	m_StateTime = 0.0f;
	SpawnShards();
	if (m_OnBreak) m_OnBreak(m_Longs, GetCenter());
}
//======================================================================================================

//位置==================================================================================================

//敵から見たカメラの方を向いた面の「右」と「上」のカメラへの向き
void LongShield::Basis(Vector3& right, Vector3& up, Vector3& toCam) const
{
	Vector3 camPos = m_HasCam ? m_CamPos : m_Center + Vector3(0.0f, 0.0f, -1000.0f);
	RibbonShapes::CameraBasis(m_Center, camPos, right, up);
	toCam = camPos - m_Center;
	if (toCam.LengthSquared() < 0.0001f) toCam = Vector3(0.0f, 0.0f, -1.0f);
	toCam.Normalize();
}

Vector3 LongShield::GetCenter() const
{
	return ToWorld(0.0f, 0.0f, 0.0f);
}

//盾の面の上の座標->ワールド座標
Vector3 LongShield::ToWorld(float x, float y, float z) const
{
	Vector3 right, up, toCam;
	Basis(right, up, toCam);
	Vector3 c = m_Center + up * kLift + toCam * kFront;
	return c + right * (x * kStretchx) + up * y + toCam * z;
}

//盾の下の方を薄くする：yがkFadeTopより上は1、kFadeBottomでkFadeMin
float LongShield::VerticalFade(float y) const
{
	float t = (y / kRadius - kFadeBottom) / (kFadeTop - kFadeBottom);	//下端０　上１
	t = std::clamp(t, 0.0f, 1.0f);
	t = t * t * (3.0f - 2.0f * t);	//スムーズ
	return kFadeMin + (1.0f - kFadeMin) * t;
}

Vector3 LongShield::GetRandomSurfacePos()
{
	float a = Rand(0.0f, kTwoPi);
	float r = std::sqrt(Rand(0.0f, 1.0f)) * kRadius * 0.7f;	//円の中に満遍なく
	return ToWorld(std::sin(a) * r, std::cos(a) * r);
}

void LongShield::Ripple(const Vector3& worldPos)
{
	if (!IsUp()) return;
	if (m_Time - m_LastRipple < kRippleInterval) return;	//これ以上出さない
	m_LastRipple = m_Time;

	Vector3 right, up, toCam;
	Basis(right, up, toCam);
	Vector3 d = worldPos - GetCenter();
	RippleFx r;
	r.pos.x = d.Dot(right);	//右へどれだけか
	r.pos.y = d.Dot(up);	//上へどれだけか
	r.size = 28.0f;
	r.life = 0.3f;
	r.color = kShieldColor;
	if (m_Ripples.size() >= 12) m_Ripples.erase(m_Ripples.begin());
	m_Ripples.push_back(r);
}

void LongShield::AddCrackLine(Crack& c, P2 start, float dir, float len, int depth)
{
	//長さに合わせて決める。ガラスの日々はほぼまっすぐでところどころ小さく折れる
	int steps = std::max(3, (int)(len / kRadius * 0.08f));
	float step = len / (float)steps;

	std::vector<P2> line;
	line.push_back(start);
	P2 p = start;
	int branchAt = (depth == 0 && Rand(0.0f, 1.0f) < 0.5f) ? (int)Rand(1.0f, (float)steps - 1.0f) : -1;

	for (int i = 0; i < steps; i++)
	{
		float kink = (Rand(0.0f, 1.0f) < 0.3f) ? Rand(-0.5f, 0.5f) : Rand(-0.12f, 0.12f);
		dir += kink;
		p.x += std::sin(dir) * step;
		p.y += std::cos(dir) * step;

		float r = std::sqrt(p.x * p.x + p.y * p.y);
		if (r > kRadius * 0.95f)	//盾の縁で止める
		{
			p.x *= kRadius * 0.95f / r;
			p.y *= kRadius * 0.95f / r;
			line.push_back(p);
			break;
		}
		line.push_back(p);

		if (i == branchAt)	//枝分かれ
		{
			float side = (Rand(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
			AddCrackLine(c, p, dir + side * Rand(0.4f, 0.9f), len * Rand(0.25f, 0.45f), depth + 1);
		}
	}
	c.lines.push_back(line);
}

void LongShield::AddCrack()
{
	Crack c;

	m_LastCrackAngle += 2.39996f + Rand(-0.3f, 0.3f);	//黄金角 ＋ 少しのずれ
	float r0 = kRadius * Rand(0.1f, 0.55f);
	c.origin.x = std::sin(m_LastCrackAngle) * r0;
	c.origin.y = std::cos(m_LastCrackAngle) * r0;

	//必要なヒビが多い時は少な目、盾が線だらけにならないように注意
	int arms = (m_Required > 6) ? 2 + (Rand(0.0f, 1.0f) < 0.5f ? 1 : 0) : 3 + (Rand(0.0f, 1.0f) < 0.5f ? 1 : 0);
	float base = Rand(0.0f, kTwoPi);
	std::vector<size_t> armIndex;	//線が c.lines の何番目か（つなぎの線に使う）
	for (int i = 0; i < arms; i++)
	{
		float ang = base + kTwoPi * (i / (float)arms) + Rand(-0.35f, 0.35f);
		float len = kRadius * Rand(0.3f, 0.6f);
		AddCrackLine(c, c.origin, ang, len, 0);
		armIndex.push_back(c.lines.size() - 1);	//枝は腕より先に足されるので、腕は最後に足された線
	}

	//蜘蛛の巣みたいに張り巡らせる
	for (int i = 0; i < arms; i++)
	{
		if (Rand(0.0f, 1.0f) > 0.6f) continue;
		const auto& a = c.lines[armIndex[i]];
		const auto& b = c.lines[armIndex[(i + 1) % arms]];
		size_t k = 2;
		if (a.size() <= k || b.size() <= k) continue;
		P2 pa = a[k], pb = b[k];
		P2 mid;	//真ん中を少しずらして、まっすぐすぎないように
		mid.x = (pa.x + pb.x) * 0.5f + Rand(-6.0f, 6.0f);
		mid.y = (pa.y + pb.y) * 0.5f + Rand(-6.0f, 6.0f);
		c.lines.push_back({ pa, mid, pb });
	}

	m_Cracks.push_back(c);

	//Flush
	RippleFx flash;
	flash.pos = c.origin;
	flash.size = 45.0f;
	flash.life = 0.22f;
	flash.color = Vector3(1.0f, 1.0f, 1.0f);
	m_Ripples.push_back(flash);
}

//破片
void LongShield::SpawnShards()
{
	for (int i = 0; i < kShardCount; i++)
	{
		float a = Rand(0.0f, kTwoPi);
		float r = std::sqrt(Rand(0.0f, 1.0f)) * kRadius * 0.85f;
		Shard s;
		s.x = std::sin(a) * r;
		s.y = std::cos(a) * r;
		s.z = 0.0f;
		float speed = Rand(250.0f, 480.0f);
		s.vx = std::sin(a) * speed;		//中心から外へ
		s.vy = std::fabs(std::cos(a)) * speed * 0.5f + Rand(120.0f, 300.0f);	//上へ
		s.vz = Rand(-260.0f, -60.0f);	//カメラの方へ
		s.rot = Rand(0.0f, kTwoPi);
		s.spin = Rand(-9.0f, 9.0f);
		s.size = Rand(18.0f, 48.0f);
		s.age = 0.0f;
		s.life = Rand(0.6f, 1.0f);
		m_Shards.push_back(s);
	}
}
//======================================================================================================

//更新
void LongShield::Update(float deltaTime, const Vector3& enemyCenter)
{
	m_Time += deltaTime;
	m_Center = enemyCenter;
	m_StateTime += deltaTime;
	m_FailFlash = std::max(0.0f, m_FailFlash - deltaTime * 3.0f);
	m_HitPulse = std::max(0.0f, m_HitPulse - deltaTime * 5.0f);

	//状態の切り替え
	if (m_State == State::Rising && m_StateTime >= kRiseTime) { m_State = State::Up; m_StateTime = 0.0f; }
	if (m_State == State::Fading && m_StateTime >= kFadeTime) { m_State = State::Down; m_StateTime = 0.0f; }
	if (m_State == State::Breaking && m_StateTime >= kBreakTime) { m_State = State::Down; m_StateTime = 0.0f; }

	for (auto& c : m_Cracks) c.age += deltaTime;

	for (auto& r : m_Ripples) r.age += deltaTime;
	m_Ripples.erase(std::remove_if(m_Ripples.begin(), m_Ripples.end(),
		[](const RippleFx& r) { return r.age >= r.life; }), m_Ripples.end());

	//破片：位置 += 速さ * 時間、だんだん遅くなり、下に落ちる
	for (auto& s : m_Shards)
	{
		s.age += deltaTime;
		s.vy -= kShardGravity * deltaTime;
		float drag = std::max(0.0f, 1.0f - 2.0f * deltaTime);
		s.vx *= drag; s.vy *= drag; s.vz *= drag;
		s.x += s.vx * deltaTime;
		s.y += s.vy * deltaTime;
		s.z += s.vz * deltaTime;
		s.rot += s.spin * deltaTime;
	}
	m_Shards.erase(std::remove_if(m_Shards.begin(), m_Shards.end(),
		[](const Shard& s) { return s.age >= s.life; }), m_Shards.end());
}

//描画
void LongShield::Flush(Camera* cam, float width, float intensity, const Vector3& glow)
{
	if (m_Pts.size() >= 2) m_Ribbon.Draw(cam, m_Pts, width, intensity, m_Time, glow);
	m_Pts.clear();
}

//六角形（角が真上）の点を足す。cx, cy は盾の面の上の中心
void LongShield::AddHex(float cx, float cy, float radius, float rot, const Vector3& color, float alpha)
{
	for (int i = 0; i <= 6; i++)
	{
		float a = rot + kTwoPi * (i / 6.0f);
		float x = cx + std::sin(a) * radius;
		float y = cy + std::cos(a) * radius;
		LongBeamRibbon::Point p;
		p.pos = ToWorld(x, y);
		p.color = RibbonShapes::MakeColor(color, alpha * VerticalFade(y));
		m_Pts.push_back(p);
	}
}

void LongShield::Draw(Camera* cam)
{
	m_CamPos = cam->GetPosition();
	m_HasCam = true;

	bool showShield = (m_State == State::Rising || m_State == State::Up || m_State == State::Fading
		|| (m_State == State::Breaking && m_StateTime < kBreakTime));
	if (!showShield && m_Shards.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);
	m_Pts.clear();

	if (showShield)
	{
		//大きさと濃さ
		float scale = 1.0f, alpha = 1.0f;
		if (m_State == State::Rising)
		{
			//少し大きく出てから戻る
			float t = std::clamp(m_StateTime / kRiseTime, 0.0f, 1.0f);
			scale = 1.0f + 0.15f * std::sin(t * RibbonShapes::kPi) - 0.6f * (1.0f - t) * (1.0f - t);
			alpha = t;
		}
		else if (m_State == State::Fading)
		{
			float f = std::clamp(m_StateTime / kFadeTime, 0.0f, 1.0f);
			scale = 1.0f + 0.08f * f;
			alpha = 1.0f - f;
		}
		else if (m_State == State::Breaking)
		{
			float f = std::clamp(m_StateTime / kBreakTime, 0.0f, 1.0f);
			scale = 1.0f + 0.25f * f;	//割れた瞬間、外へ弾けて消える
			alpha = 1.0f - f;
		}
		scale *= 1.0f + 0.04f * m_HitPulse;	//ヒビが入った瞬間に少し震える

		//色
		Vector3 col = m_Failed ? kFailColor : kShieldColor;
		col = col * (1.0f - m_FailFlash) + Vector3(1.0f, 0.3f, 0.25f) * m_FailFlash;
		float glowI = m_Failed ? 0.7f : 1.0f;

		const float R = kRadius * scale;

		//外側の太い縁と、ぼんやりした光
		AddHex(0.0f, 0.0f, R, 0.0f, col, alpha);
		Flush(cam, 10.0f, 1.3f * glowI, col);
		AddHex(0.0f, 0.0f, R * 0.96f, 0.0f, col, alpha * 0.25f);
		Flush(cam, 28.0f, 0.6f * glowI, col);

		//中のハチの巣（真ん中に1つ、まわりに6つ）
		float a = R * 0.3f;					//小さい六角形の大きさ
		float d = a * 1.7320508f;			//隣の中心までの距離（√3 × 大きさ）
		AddHex(0.0f, 0.0f, a, 0.0f, col, alpha * 0.35f);
		Flush(cam, 2.5f, 0.8f * glowI, col);
		for (int k = 0; k < 6; k++)
		{
			float ang = kTwoPi * (k / 6.0f) + RibbonShapes::kPi / 6.0f;	//30度ずらした6方向
			AddHex(std::sin(ang) * d, std::cos(ang) * d, a, 0.0f, col, alpha * 0.35f);
			Flush(cam, 2.5f, 0.8f * glowI, col);
		}

		//ヒビ(入った直後は太く明るい）
		for (const auto& c : m_Cracks)
		{
			float grow = std::clamp(c.age / kCrackGrow, 0.0f, 1.0f);
			float fresh = std::max(0.0f, 1.0f - c.age * 3.0f);
			for (const auto& line : c.lines)
			{
				if (line.size() < 2) continue;
				float shown = grow * (float)(line.size() - 1);	//何区間目まで見せるか
				int full = (int)shown;

				for (int pass = 0; pass < 2; pass++)	//0：外側のぼんやりした光、1：細い芯
				{
					if (pass == 0 && fresh <= 0.0f) continue;	//ぼんやりした光は入った直後だけ（描く回数を減らす）
					for (size_t i = 0; i < line.size(); i++)
					{
						P2 p = line[i];
						if ((int)i > full)	//伸びている途中：最後の区間は途中まで
						{
							float f = shown - (float)full;
							P2 q = line[i - 1];
							p.x = q.x + (p.x - q.x) * f;
							p.y = q.y + (p.y - q.y) * f;
						}
						float taper = 1.0f - 0.65f * (i / (float)(line.size() - 1));	//先へ行くほど薄く
						LongBeamRibbon::Point pt;
						pt.pos = ToWorld(p.x * scale, p.y * scale);
						pt.color = RibbonShapes::MakeColor(kCrackColor, alpha * taper * VerticalFade(p.y));
						m_Pts.push_back(pt);
						if ((int)i > full) break;
					}
					if (pass == 0) Flush(cam, 10.0f, 0.5f * fresh, kShieldColor);
					else           Flush(cam, 2.5f + 2.0f * fresh, 1.3f + 0.8f * fresh, kShieldColor);
				}
			}
		}

		//はじいた所の波紋（小さい六角形が広がって消える）
		for (const auto& r : m_Ripples)
		{
			float k = r.age / 0.35f;
			AddHex(r.pos.x, r.pos.y, 10.0f + 35.0f * k, 0.0f, kShieldColor, alpha * (1.0f - k));
			Flush(cam, 5.0f, 1.2f, kShieldColor);
		}
	}

	//破片：小さい三角形が回りながら飛ぶ
	for (const auto& s : m_Shards)
	{
		float fade = 1.0f - std::clamp(s.age / s.life, 0.0f, 1.0f);
		for (int i = 0; i <= 3; i++)
		{
			float ang = s.rot + kTwoPi * (i / 3.0f);
			float y = s.y + std::cos(ang) * s.size;
			LongBeamRibbon::Point q;
			q.pos = ToWorld(s.x + std::sin(ang) * s.size, y, s.z);
			q.color = RibbonShapes::MakeColor(Vector3(0.8f, 0.92f, 1.0f), fade * VerticalFade(y));
			m_Pts.push_back(q);
		}
		float w = kShaderFilled ? s.size * 0.2f : 300.5f;			// 塗りつぶし風：辺の線を太くして中を埋める
		float lightI = kShaderFilled ? 0.8f : 1.5f;			// 太いと明るすぎるので少し下げる
		Flush(cam, w, lightI, kShieldColor);
	}

	Renderer::SetBlendState(BS_NONE);
}