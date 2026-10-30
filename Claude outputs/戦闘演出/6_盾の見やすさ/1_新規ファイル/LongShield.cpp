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
	const Vector3 kShieldColor(0.45f, 0.75f, 1.0f);		// 盾の色（青白）
	const Vector3 kFailColor(0.55f, 0.55f, 0.65f);		// 失敗した盾の色（くすんだ灰色）
	const Vector3 kCrackColor(1.0f, 1.0f, 1.0f);		// ヒビの芯の色
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

// ============================================================
// 出来事
// ============================================================
void LongShield::BeginLong(int checkpoints)
{
	// 盾が無い（消えかけ・割れた直後も含む）なら、新しい盾を張る
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
	// 盾が張られている時に始まったロングは、同じ盾に足し合わせる
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
			m_Failed = true;		// この盾はもう割れない
			m_FailFlash = 1.0f;
			if (m_OnFail) m_OnFail();
		}
		return;
	}
	if (m_Failed) return;	// 失敗した後は、ヒビも入れない

	m_Done++;
	AddCrack();
	m_HitPulse = 1.0f;
	if (m_OnCrack) m_OnCrack(m_Done, m_Required);

	if (m_Done >= m_Required) Break();	// 全部うまくいった
}

void LongShield::EndLong()
{
	m_Active = std::max(0, m_Active - 1);
	if (m_Active == 0 && IsUp())
	{
		// 割れないまま全部のロングが終わった：静かに消える
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

// ============================================================
// 位置
// ============================================================
// 敵から見て、カメラの方を向いた面の「右」「上」と、カメラへの向き
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

// 盾の面の上の座標 → ワールド座標
Vector3 LongShield::ToWorld(float x, float y, float z) const
{
	Vector3 right, up, toCam;
	Basis(right, up, toCam);
	Vector3 c = m_Center + up * kLift + toCam * kFront;
	return c + right * x + up * y + toCam * z;
}

// 盾の下の方ほど薄くする：y が kFadeTop より上は 1、kFadeBottom で kFadeMin
float LongShield::VerticalFade(float y) const
{
	float t = (y / kRadius - kFadeBottom) / (kFadeTop - kFadeBottom);	// 下端 0 → 上 1
	t = std::clamp(t, 0.0f, 1.0f);
	t = t * t * (3.0f - 2.0f * t);	// なめらかに
	return kFadeMin + (1.0f - kFadeMin) * t;
}

// 攻撃が当たる所：盾の上半分に寄せる（下半分はレーンに近いので、光をそこに集めない）
Vector3 LongShield::GetRandomSurfacePos()
{
	float a = Rand(-1.3f, 1.3f);	// 真上から左右に約75度まで
	float r = std::sqrt(Rand(0.0f, 1.0f)) * kRadius * 0.7f;
	return ToWorld(std::sin(a) * r, std::cos(a) * r);
}

void LongShield::Ripple(const Vector3& worldPos)
{
	if (!IsUp()) return;
	if (m_Time - m_LastRipple < kRippleInterval) return;	// 出しすぎない
	m_LastRipple = m_Time;

	Vector3 right, up, toCam;
	Basis(right, up, toCam);
	Vector3 d = worldPos - GetCenter();
	RippleFx r;
	r.pos.x = d.Dot(right);	// 右へどれだけか
	r.pos.y = d.Dot(up);	// 上へどれだけか
	r.size = 28.0f;
	r.life = 0.3f;
	r.color = kShieldColor;
	if (m_Ripples.size() >= 12) m_Ripples.erase(m_Ripples.begin());
	m_Ripples.push_back(r);
}

// ============================================================
// ヒビ：ガラスの割れ方をまねる
//   1つの起点から放射状に腕を伸ばし（たまに枝分かれ）、隣り合う腕どうしを短い線でつなぐ（蜘蛛の巣）
// ============================================================
void LongShield::AddCrackLine(Crack& c, P2 start, float dir, float len, int depth)
{
	// 長さに合わせて歩数を決める。ガラスのヒビはほぼまっすぐで、ところどころ小さく折れる
	int steps = std::max(3, (int)(len / (kRadius * 0.08f)));
	float step = len / (float)steps;

	std::vector<P2> line;
	line.push_back(start);
	P2 p = start;
	int branchAt = (depth == 0 && Rand(0.0f, 1.0f) < 0.5f) ? (int)Rand(1.0f, (float)steps - 1.0f) : -1;

	for (int i = 0; i < steps; i++)
	{
		float kink = (Rand(0.0f, 1.0f) < 0.3f) ? Rand(-0.5f, 0.5f) : Rand(-0.12f, 0.12f);	// ときどき大きく折れる
		dir += kink;
		p.x += std::sin(dir) * step;
		p.y += std::cos(dir) * step;

		float r = std::sqrt(p.x * p.x + p.y * p.y);
		if (r > kRadius * 0.95f)	// 盾の縁で止める
		{
			p.x *= kRadius * 0.95f / r;
			p.y *= kRadius * 0.95f / r;
			line.push_back(p);
			break;
		}
		line.push_back(p);

		if (i == branchAt)	// 枝分かれ：途中から斜めに短い線
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

	// 起点：前のヒビから黄金角だけ回した方向に置く（ヒビが偏らずに盾全体に散らばる）
	m_LastCrackAngle += 2.39996f;
	float r0 = kRadius * Rand(0.1f, 0.55f);
	c.origin.x = std::sin(m_LastCrackAngle) * r0;
	c.origin.y = std::cos(m_LastCrackAngle) * r0;

	// 腕の本数：必要なヒビが多い時は少なめにして、盾が線だらけにならないようにする
	int arms = (m_Required > 6) ? 2 + (Rand(0.0f, 1.0f) < 0.5f ? 1 : 0) : 3 + (Rand(0.0f, 1.0f) < 0.5f ? 1 : 0);
	float base = Rand(0.0f, kTwoPi);
	std::vector<size_t> armIndex;	// 腕の線が c.lines の何番目か（つなぎの線に使う）
	for (int i = 0; i < arms; i++)
	{
		float ang = base + kTwoPi * (i / (float)arms) + Rand(-0.35f, 0.35f);
		float len = kRadius * Rand(0.3f, 0.6f);
		AddCrackLine(c, c.origin, ang, len, 0);
		armIndex.push_back(c.lines.size() - 1);	// 枝は腕より先に足されるので、腕は最後に足された線
	}

	// 蜘蛛の巣：隣り合う腕の、起点から少し離れた点どうしを短い線でつなぐ
	for (int i = 0; i < arms; i++)
	{
		if (Rand(0.0f, 1.0f) > 0.6f) continue;
		const auto& a = c.lines[armIndex[i]];
		const auto& b = c.lines[armIndex[(i + 1) % arms]];
		size_t k = 2;
		if (a.size() <= k || b.size() <= k) continue;
		P2 pa = a[k], pb = b[k];
		P2 mid;	// 真ん中を少しずらして、まっすぐすぎないように
		mid.x = (pa.x + pb.x) * 0.5f + Rand(-6.0f, 6.0f);
		mid.y = (pa.y + pb.y) * 0.5f + Rand(-6.0f, 6.0f);
		c.lines.push_back({ pa, mid, pb });
	}

	m_Cracks.push_back(c);

	// 起点がパッと光る
	RippleFx flash;
	flash.pos = c.origin;
	flash.size = 45.0f;
	flash.life = 0.22f;
	flash.color = Vector3(1.0f, 1.0f, 1.0f);
	m_Ripples.push_back(flash);
}

// 割れた時の破片：上と横、敵の方へ散らす（カメラの方・レーンの方へは飛ばさない）
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
		float speed = Rand(200.0f, 480.0f);
		s.vx = std::sin(a) * speed;										// 横へ
		s.vy = std::fabs(std::cos(a)) * speed * 0.5f + Rand(120.0f, 300.0f);	// 上へ（下向きには飛ばさない）
		s.vz = Rand(-260.0f, -60.0f);									// 敵の方（奥）へ
		s.rot = Rand(0.0f, kTwoPi);
		s.spin = Rand(-8.0f, 8.0f);
		s.size = Rand(10.0f, 26.0f);
		s.age = 0.0f;
		s.life = Rand(0.4f, 0.65f);
		m_Shards.push_back(s);
	}
}

// ============================================================
// 更新
// ============================================================
void LongShield::Update(float deltaTime, const Vector3& enemyCenter)
{
	m_Time += deltaTime;
	m_Center = enemyCenter;
	m_StateTime += deltaTime;
	m_FailFlash = std::max(0.0f, m_FailFlash - deltaTime * 3.0f);
	m_HitPulse = std::max(0.0f, m_HitPulse - deltaTime * 4.0f);

	// 状態の切り替え
	if (m_State == State::Rising && m_StateTime >= kRiseTime) { m_State = State::Up; m_StateTime = 0.0f; }
	if (m_State == State::Fading && m_StateTime >= kFadeTime) { m_State = State::Down; m_StateTime = 0.0f; }
	if (m_State == State::Breaking && m_StateTime >= kBreakTime) { m_State = State::Down; m_StateTime = 0.0f; }

	for (auto& c : m_Cracks) c.age += deltaTime;

	for (auto& r : m_Ripples) r.age += deltaTime;
	m_Ripples.erase(std::remove_if(m_Ripples.begin(), m_Ripples.end(),
		[](const RippleFx& r) { return r.age >= r.life; }), m_Ripples.end());

	// 破片：位置 ＋＝ 速さ × 時間、だんだん遅くなり、下に落ちる
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

// ============================================================
// 描画
// ============================================================
void LongShield::Flush(Camera* cam, float width, float intensity, const Vector3& glow)
{
	if (m_Pts.size() >= 2) m_Ribbon.Draw(cam, m_Pts, width, intensity, m_Time, glow);
	m_Pts.clear();
}

// 六角形（角が真上）の点を足す。cx, cy は盾の面の上の中心。下の方ほど薄くする
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
		// ── 大きさと濃さ ──
		float scale = 1.0f, alpha = 1.0f;
		if (m_State == State::Rising)
		{
			float t = std::clamp(m_StateTime / kRiseTime, 0.0f, 1.0f);
			scale = 1.0f + 0.12f * std::sin(t * RibbonShapes::kPi) - 0.5f * (1.0f - t) * (1.0f - t);
			alpha = t;
		}
		else if (m_State == State::Fading)
		{
			float f = std::clamp(m_StateTime / kFadeTime, 0.0f, 1.0f);
			scale = 1.0f + 0.06f * f;
			alpha = 1.0f - f;
		}
		else if (m_State == State::Breaking)
		{
			float f = std::clamp(m_StateTime / kBreakTime, 0.0f, 1.0f);
			scale = 1.0f + 0.2f * f;
			alpha = 1.0f - f;
		}
		scale *= 1.0f + 0.03f * m_HitPulse;	// ヒビが入った瞬間に少し震える

		// ── 色と濃さ：ふだんは薄く、ヒビが入った瞬間だけ明るく ──
		Vector3 col = m_Failed ? kFailColor : kShieldColor;
		col = col * (1.0f - m_FailFlash) + Vector3(1.0f, 0.3f, 0.25f) * m_FailFlash;
		float body = alpha * (kBaseAlpha + (1.0f - kBaseAlpha) * m_HitPulse);
		float glowI = m_Failed ? 0.6f : 1.0f;
		const float R = kRadius * scale;

		// 外側の縁と、ぼんやりした光
		AddHex(0.0f, 0.0f, R, 0.0f, col, body);
		Flush(cam, 7.0f, 1.2f * glowI, col);
		AddHex(0.0f, 0.0f, R * 0.96f, 0.0f, col, body * 0.25f);
		Flush(cam, 22.0f, 0.5f * glowI, col);

		// 中のハチの巣（真ん中に1つ、まわりに6つ。かなり薄く）
		float a = R * 0.3f;
		float d = a * 1.7320508f;
		AddHex(0.0f, 0.0f, a, 0.0f, col, body * 0.3f);
		Flush(cam, 2.5f, 0.7f * glowI, col);
		for (int k = 0; k < 6; k++)
		{
			float ang = kTwoPi * (k / 6.0f) + RibbonShapes::kPi / 6.0f;
			AddHex(std::sin(ang) * d, std::cos(ang) * d, a, 0.0f, col, body * 0.3f);
			Flush(cam, 2.5f, 0.7f * glowI, col);
		}

		// ── ヒビ：起点から伸びていく。起点ほど濃く、先ほど薄い ──
		for (const auto& c : m_Cracks)
		{
			float grow = std::clamp(c.age / kCrackGrow, 0.0f, 1.0f);	// どこまで伸びたか
			float fresh = std::max(0.0f, 1.0f - c.age * 3.0f);			// 入った直後ほど 1

			for (const auto& line : c.lines)
			{
				if (line.size() < 2) continue;
				float shown = grow * (float)(line.size() - 1);	// 何区間目まで見せるか
				int full = (int)shown;

				for (int pass = 0; pass < 2; pass++)	// 0：外側のぼんやりした光、1：細い芯
				{
					if (pass == 0 && fresh <= 0.0f) continue;	// ぼんやりした光は入った直後だけ（描く回数を減らす）
					for (size_t i = 0; i < line.size(); i++)
					{
						P2 p = line[i];
						if ((int)i > full)	// 伸びている途中：最後の区間は途中まで
						{
							float f = shown - (float)full;
							P2 q = line[i - 1];
							p.x = q.x + (p.x - q.x) * f;
							p.y = q.y + (p.y - q.y) * f;
						}
						float taper = 1.0f - 0.65f * (i / (float)(line.size() - 1));	// 先へ行くほど薄く
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

		// ── 波紋（はじいた所）と、ヒビの起点の光 ──
		for (const auto& r : m_Ripples)
		{
			float k = r.age / r.life;
			AddHex(r.pos.x, r.pos.y, r.size * (0.3f + 0.7f * k), 0.0f, r.color, alpha * (1.0f - k) * 0.6f);
			Flush(cam, 4.0f, 1.2f, kShieldColor);
		}
	}

	// ── 破片：小さい三角形が回りながら、上と横へ散る ──
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
		Flush(cam, 3.5f, 1.3f, kShieldColor);
	}

	Renderer::SetBlendState(BS_NONE);
}
