#include "JudgeSquareFx.h"
#include "RibbonShapes.h"
#include "GameEvent.h"
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void JudgeSquareFx::Init()
{
	m_Ribbon.Init();
	m_Pts.reserve(8);
	m_Squares.reserve(kMaxSquares);
	m_Stars.reserve(kMaxSquares);
	m_Rng.seed(std::random_device{}());

	m_JudgeSub = GameEvent::OnJudge().Subscribe([this](const GameEvent::Judge& e)
		{
			//タップ・フリック・トレース：四角
			if (!e.isLong)
			{
				SpawnSquare(e.pos, e.rank);
				return;
			}

			//ロング：しばらく来ていなかったら押し始め->四角。続けてきていたら押している途中->星
			if (e.lane < 0 || e.lane >= 6) return;
			bool start = (m_Time - m_LastLongTick[e.lane]) > kLongGap;
			m_LastLongTick[e.lane] = m_Time;
			if (start)
			{
				SpawnSquare(e.pos, e.rank);
				m_LongTickCount[e.lane] = 0;
				return;
			}
			if (++m_LongTickCount[e.lane] % kStarEvery == 0)
			{
				Vector3 c = RankColor(e.rank);
				SpawnStar(e.pos, c + (Vector3(1.0f, 1.0f, 1.0f) - c) * 0.35f);	//少し白を混ぜて明るく
			}
		});
}

float JudgeSquareFx::Rand(float a, float b)
{
	std::uniform_real_distribution<float> d(a, b);
	return d(m_Rng);
}

void JudgeSquareFx::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;
	dt = std::min(dt, 0.1f);

	for (auto& s : m_Squares) s.age += dt;
	m_Squares.erase(std::remove_if(m_Squares.begin(), m_Squares.end(),
		[](const Square& s) { return s.age >= kSquareTime; }), m_Squares.end());

	for (auto& s : m_Stars)
	{
		s.age += dt;
		s.vel *= std::max(0.0f, 1.0f - 1.5f * dt);	//だんだんゆっくり
		s.pos += s.vel * dt;
		s.rot += s.spin * dt;
	}
	m_Stars.erase(std::remove_if(m_Stars.begin(), m_Stars.end(),
		[](const Star& s) { return s.age >= kStarTime; }), m_Stars.end());
}

void JudgeSquareFx::Draw(Camera* cam)
{
	if (m_Squares.empty() && m_Stars.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);

	//四角：最初は速く広がって、だんだんゆっくり。広がりながら薄く・細くなる
	for (const auto& s : m_Squares)
	{
		float u = s.age / kSquareTime;
		float e = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
		float size = (kSquareStart + (kSquareEnd - kSquareStart) * e) * s.scale;
		float alpha = std::pow(1.0f - u, 1.5f);
		float width = kSquareLine * (1.0f - 0.5f * u);
		DrawSquare(cam, s.pos, size, s.color, alpha, width);

		//少し遅れて、内側にうすい四角をもう1つ（二重にして奥行きを出す）
		float u2 = std::clamp((s.age - 0.05f) / kSquareTime, 0.0f, 1.0f);
		if (u2 > 0.0f)
		{
			float e2 = 1.0f - (1.0f - u2) * (1.0f - u2) * (1.0f - u2);
			float size2 = (kSquareStart * 0.6f + (kSquareEnd * 0.75f - kSquareStart * 0.6f) * e2) * s.scale;
			DrawSquare(cam, s.pos, size2, s.color, alpha * 0.45f, width * 0.6f);
		}
	}

	//星：キラッと出て、回りながら昇って消える
	for (const auto& s : m_Stars)
	{
		float u = s.age / kStarTime;
		float pop = std::min(1.0f, s.age / 0.08f);	//出た瞬間にふくらむ
		float size = s.size * pop * (1.0f - 0.4f * u);
		float alpha = (u < 0.6f) ? 1.0f : 1.0f - (u - 0.6f) / 0.4f;
		alpha *= 0.8f + 0.2f * std::sin(s.age * 40.0f);	//瞬く

		Vector3 right, up;
		RibbonShapes::CameraBasis(s.pos, cam->GetPosition(), right, up);
		Vector3 d1 = right * std::cos(s.rot) + up * std::sin(s.rot);
		Vector3 d2 = right * -std::sin(s.rot) + up * std::cos(s.rot);
		DrawStreak(cam, s.pos, d1, size * 1.6f, s.color, alpha, size * 0.22f, 1.0f);	//光の筋（十字）
		DrawStreak(cam, s.pos, d2, size * 1.6f, s.color, alpha, size * 0.22f, 1.0f);
		DrawStreak(cam, s.pos, d1, size * 0.5f, s.color, alpha * 0.5f, size * 1.0f, 0.4f);	//まわりのぼんやりした光
		DrawStreak(cam, s.pos, d1, size * 0.15f, Vector3(1.0f, 1.0f, 1.0f), alpha, size * 0.3f, 1.0f);	//白い芯
	}
}

void JudgeSquareFx::Uninit()
{
	GameEvent::OnJudge().Unsubscribe(m_JudgeSub);
	m_Squares.clear();
	m_Stars.clear();
}

void JudgeSquareFx::SpawnSquare(const Vector3& pos, int rank)
{
	if ((int)m_Squares.size() >= kMaxSquares) m_Squares.erase(m_Squares.begin());
	Square s;
	s.pos = pos;
	s.color = RankColor(rank);
	//s.scale = (rank == 0) ? 1.15f : (rank == 1) ? 1.0f : 0.85f;	//weakほど大きくしたかったら使って
	m_Squares.push_back(s);
}

void JudgeSquareFx::SpawnStar(const Vector3& pos, const Vector3& color)
{
	if ((int)m_Stars.size() >= kMaxStars) m_Stars.erase(m_Stars.begin());
	Star s;
	s.pos = pos + Vector3(Rand(-40.0f, 40.0f), Rand(0.0f, 20.0f), 0.0f);
	s.vel = Vector3(Rand(-50.0f, 50.0f), kStarRise * Rand(0.8f, 1.2f), 0.0f);
	s.color = color;
	s.size = kStarSize * Rand(0.8f, 1.3f);
	s.rot = Rand(0.0f, 1.57f);
	s.spin = Rand(-4.0f, 4.0f);
	m_Stars.push_back(s);
}

void JudgeSquareFx::DrawLine(Camera* cam, const Vector3& a, const Vector3& b, const Vector3& color, float alpha, float width, float intensity)
{
	m_Pts.clear();
	LongBeamRibbon::Point p;
	p.color = RibbonShapes::MakeColor(color, alpha);
	p.pos = a;              m_Pts.push_back(p);
	p.pos = (a + b) * 0.5f; m_Pts.push_back(p);
	p.pos = b;              m_Pts.push_back(p);
	m_Ribbon.Draw(cam, m_Pts, width, intensity, 0.0f, color);
}

void JudgeSquareFx::DrawStreak(Camera* cam, const Vector3& center, const Vector3& dir, float len, const Vector3& color, float alpha, float width, float intensity)
{
	m_Pts.clear();
	LongBeamRibbon::Point p;
	p.pos = center - dir * len; p.color = RibbonShapes::MakeColor(color, 0.0f);  m_Pts.push_back(p);
	p.pos = center;             p.color = RibbonShapes::MakeColor(color, alpha); m_Pts.push_back(p);
	p.pos = center + dir * len; p.color = RibbonShapes::MakeColor(color, 0.0f);  m_Pts.push_back(p);
	m_Ribbon.Draw(cam, m_Pts, width, intensity, 0.0f, color);
}

void JudgeSquareFx::DrawSquare(Camera* cam, const Vector3& center, float size, const Vector3& color, float alpha, float width)
{
	Vector3 right, up;
	RibbonShapes::CameraBasis(center, cam->GetPosition(), right, up);
	float h = size * 0.5f;
	float e = h + width * 0.5f;
	DrawLine(cam, center - right * e + up * h, center + right * e + up * h, color, alpha, width, 1.2f);	//上
	DrawLine(cam, center - right * e - up * h, center + right * e - up * h, color, alpha, width, 1.2f);	//下
	DrawLine(cam, center - right * h + up * e, center - right * h - up * e, color, alpha, width, 1.2f);	//左
	DrawLine(cam, center + right * h + up * e, center + right * h - up * e, color, alpha, width, 1.2f);	//右
}
