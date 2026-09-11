#include "HomingBeamSystem.h"
#include "ModelUtil.h"
#include "Renderer.h"
#include "Camera.h"
#include "sound.h"
#include "Enemy.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace DirectX::SimpleMath;

// ============================================================
// Init : ビーム用モデルのロード（元 ChartObject::Init の該当ブロックを移設）
// ============================================================
void HomingBeamSystem::Init()
{
	// ── ビーム用モデルのロード ──
	// 棒状のFBXを想定。無い場合、ビームは一切描画されない（Draw内でreturnする）。
	m_BeamModelLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/Beam/beam.fbx", "assets/model/beam",
		m_BeamMeshRenderer, m_BeamShader, m_BeamSubsets, m_BeamMaterials, m_BeamTextures);
	if (!m_BeamModelLoaded)
		OutputDebugStringA("[HomingBeamSystem] Beam model not found. Beam will not be drawn.\n");
	if (m_BeamModelLoaded)
	{
		m_BeamShader.Create("shader/litTextureVS.hlsl", "shader/beamPS.hlsl");
		m_BeamShader.CreatePSConstantBuffer(sizeof(BeamShaderParms));

		if (!m_BeamNoiseTexture.Load("assets/texture/noise.png"))
		{
			std::cout << ("[HomingBeamSystem] Beam noise texture not found.\n");
		}
	}
}

// ============================================================
// Update : 毎フレーム呼ばれる、ビーム全体の更新処理（元 ChartObject::UpdateHomingBeams）。
//   1. 各ビームの状態遷移：
//        Homing（探索中。タンタンタン→ロック確定）
//        → Fired（発射済み。一定間隔で命中/回避判定）
//        → Resolved（判定終了）
//   playerSide / playerSideAnchor は ChartObject 側で計算して渡してもらう
//   （Player/JudgeCirclesへの依存を持たせないため）。
// ============================================================
void HomingBeamSystem::Update(float deltaTime, float musicTime, int playerSide,
	const Vector3& playerSideAnchor, Sound& sound, ScreenEffects& effects)
{
	m_BeamShaderTime += deltaTime;

	// ── 各ビームの状態遷移 ──
	for (auto& beam : m_HomingBeams)
	{
		if (beam.state == ChartHomingBeam::State::Resolved) continue;
		if (musicTime < beam.chargeTime) continue;

		{
			float total = beam.lockTime - beam.chargeTime;
			if (total < 0.01f) total = 0.01f;
			float beatInterval = total / ChartHomingBeam::kSearchBeatCount;
			while (beam.searchBeatsPlayed < ChartHomingBeam::kSearchBeatCount)
			{
				float beatTime = beam.chargeTime + beatInterval * beam.searchBeatsPlayed;
				if (musicTime >= beatTime)
				{
					sound.Play(SOUND_BEAM_SARCH);
					beam.searchBeatsPlayed++;
				}
				else break;
			}
		}

		if (beam.state == ChartHomingBeam::State::Homing)
		{
			if (!beam.judgmentLocked && beam.searchBeatsPlayed >= ChartHomingBeam::kSearchBeatCount)
			{
				beam.judgmentLocked = true;
				if (!beam.lockSoundPlayed)
				{
					sound.Play(SOUND_BEAM_DECITION);
					beam.lockSoundPlayed = true;
				}
			}

			if (!beam.judgmentLocked)
			{
				beam.lockedSide = playerSide;
				beam.lockedPos = playerSideAnchor;
				beam.lockedPos.x += playerSide * 150.0f; // 旧m_PlayerMoveRange相当。必要ならPlayer::GetMoveRange()を公開して参照
			}

			if (musicTime >= beam.lockTime)
			{
				if (!beam.fireSoundPlayed)
				{
					sound.Play(SOUND_BEAM_FIRING);
					beam.fireSoundPlayed = true;
				}
				beam.state = ChartHomingBeam::State::Fired;

				// 初回の判定タイミングは lockTime から開始する（必要なら + interval）
				beam.nextCountTime = beam.lockTime;
			}
		}
		else if (beam.state == ChartHomingBeam::State::Fired)
		{
			if (musicTime >= beam.hitTime)
			{
				beam.state = ChartHomingBeam::State::Resolved;
			}
			else
			{
				// kCountInterval の安全コピー
				const float interval = ChartHomingBeam::kCountInterval;
				if (interval <= 0.0f) {
					// 安全策：不正値なら解決して抜ける
					beam.state = ChartHomingBeam::State::Resolved;
				}
				else {
					// 複数カウント分を取りこぼさないよう while で処理する
					while (beam.nextCountTime < beam.hitTime && musicTime >= beam.nextCountTime)
					{
						bool dodged = (playerSide != beam.lockedSide);
						if (dodged)
						{
							//m_Perfect++;
							effects.TriggerScreenShake(0.1f, 4.0f);
						}
						else
						{
							//m_Miss++;
							effects.TriggerScreenShake(0.25f, 15.0f);
							effects.TriggerFlash(0.15f);
						}

						// advance (繰り越しを残すため -= ではなく +=)
						beam.nextCountTime += interval;

						// safety: ループが止まらない可能性を防ぐ（万一のため）
						if (interval <= 0.0f) break;
					}
				}
			}
		}
	}
}

// ============================================================
// DrawBeamSegment : from→to の間にビームモデルを引き伸ばして1本描画する。
//   モデルのローカルZ軸方向をdir(from→to方向)に向け、
//   X/Yスケールで太さを、Zスケールで長さを調整する。
// ============================================================
void HomingBeamSystem::DrawBeamSegment(Camera* cam,
	const Vector3& from, const Vector3& to,
	float thicknessX, float thicknessY)
{
	if (!m_BeamModelLoaded) return;

	Vector3 dir = to - from;
	float len = dir.Length();
	if (len < 0.01f) return;
	dir.Normalize();

	Vector3 upHint = Vector3::Up;
	if (fabsf(dir.Dot(upHint)) > 0.98f) upHint = Vector3::Right;

	Vector3 right = upHint.Cross(dir);
	right.Normalize();
	Vector3 up = dir.Cross(right);
	up.Normalize();

	Vector3 mid = from + dir * (len * 0.5f);

	// CreateWorldに頼らず、基底ベクトルから直接ワールド行列を組む
	// （モデルのローカルZ軸が「長さ方向」である前提。もし違う軸なら
	//   下のbasis内のright/up/dirの並び順を入れ替えて調整してください）
	Matrix basis(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		dir.x, dir.y, dir.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	Renderer::SetBlendState(BS_ADDITIVE);

	auto drawLayer = [&](float scaleMul)
		{
			Matrix world =
				Matrix::CreateScale(thicknessX * scaleMul, thicknessY * scaleMul, len / m_BeamModelLength) *
				basis *
				Matrix::CreateTranslation(mid);
			ModelUtil::DrawModel(m_BeamMeshRenderer, m_BeamShader, m_BeamSubsets, m_BeamMaterials, m_BeamTextures, world);
		};
	// 外側から描くと、内側の明るいコアが最後に重なって一番目立つ
	drawLayer(0.3f);   // グロー：太く薄く広がる外周
	drawLayer(0.6f);   // ミッド：中間の明るさ
	drawLayer(1.0f);   // コア：細く一番明るい芯

	Renderer::SetBlendState(BS_NONE);
}

void HomingBeamSystem::DrawBeamSegmentWithColor(Camera* cam, const Vector3& from, const Vector3& to, float thicknessX, float thicknessY, const Vector3& baseColor, float intensity)
{
	if (!m_BeamModelLoaded) return;

	Vector3 dir = to - from;
	float len = dir.Length();
	if (len < 0.01f) return;
	dir.Normalize();

	Vector3 upHint = Vector3::Up;
	if (fabsf(dir.Dot(upHint)) > 0.98f) upHint = Vector3::Right;
	Vector3 right = upHint.Cross(dir);
	right.Normalize();
	Vector3 up = dir.Cross(right);
	up.Normalize();
	Vector3 mid = from + dir * (len * 0.5f);

	Matrix basis(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		dir.x, dir.y, dir.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	//シェーダー適用
	m_BeamShader.SetGPU();

	BeamShaderParms parms = {};
	parms.time = m_BeamShaderTime;
	parms.intensity = intensity;
	parms.baseColor = DirectX::SimpleMath::Vector4(baseColor.x, baseColor.y, baseColor.z, 1.0f);
	m_BeamShader.UpdatePSConstantBuffer(&parms, sizeof(parms));

	//ノイズテクスチャをt0にセット
	m_BeamNoiseTexture.SetGPU();

	Matrix world =
		Matrix::CreateScale(thicknessX, thicknessY, len / m_BeamModelLength) *
		basis *
		Matrix::CreateTranslation(from);

	Matrix worldCopy = world;
	Renderer::SetWorldMatrix(&worldCopy);
	m_BeamMeshRenderer.BeforeDraw();
	for (size_t i = 0; i < m_BeamSubsets.size(); i++)
	{
		m_BeamMeshRenderer.DrawSubset(
			m_BeamSubsets[i].IndexNum,
			m_BeamSubsets[i].IndexBase,
			m_BeamSubsets[i].VertexBase
		);
	}
}

void HomingBeamSystem::DrawJudggedBeam(Camera* cam, const Vector3& from, const Vector3& to, float thickness, const Vector3& color, float intensity, int segments, float jitter)
{
	Vector3 prev = from;
	for (int i = 1; i <= segments; i++)
	{
		float t = (float)i / segments;
		Vector3 base = from + (to - from) * t;
		if (i != segments)
		{
			float ox = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * jitter;
			float oy = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * jitter;
			base += Vector3(ox, oy, 0.0f);
		}
		DrawBeamSegmentWithColor(cam, prev, base, thickness, thickness, color, intensity);
		prev = base;
	}
}

// ============================================================
// Draw : 全ビームの描画をまとめて行う（元 ChartObject::DrawHomingBeams）。
//   Homing中：探索中の「バラけた線」が収束していく様子を表現
//   Fired中　：ロック位置へ向けて1本の太いビームを描画
// ============================================================
void HomingBeamSystem::Draw(Camera* cam, float musicTime, Enemy* enemyRef,
	const Vector3& playerSideAnchor)
{
	if (!m_BeamModelLoaded) return;
	if (m_HomingBeams.empty()) return;

	// ビームの発射元座標。Enemyの参照が無ければ仮の固定値を使う
	Vector3 beamOrigin = enemyRef
		? (enemyRef->GetPosition() + Vector3(0.0f, 50.0f, 0.0f))
		: Vector3(0.0f, 0.0f, 500.0f);

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ALPHABLEND);
	//Renderer::SetBlendState(BS_NONE);

	for (auto& beam : m_HomingBeams)
	{
		if (beam.state == ChartHomingBeam::State::Resolved) continue;
		if (musicTime < beam.chargeTime) continue;

		// ロック確定済みならロック座標へ、まだならプレイヤーの現在位置へ収束させる
		Vector3 lockTarget = beam.judgmentLocked
			? beam.lockedPos
			: playerSideAnchor;

		Vector3 color = (beam.state == ChartHomingBeam::State::Fired)
			? Vector3(1.0f, 0.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);

		if (beam.state == ChartHomingBeam::State::Homing)
		{
			// 収束の進行度t(0=バラバラ, 1=収束完了)をease-inで計算
			float total = beam.lockTime - beam.chargeTime;
			if (total < 0.01f) total = 0.01f;
			float t = std::clamp((musicTime - beam.chargeTime) / total, 0.0f, 1.0f);
			float easedT = t * t;

			// 散らばった候補線を描画。tが進むほどlockTargetへ寄っていく
			for (int i = 0; i < ChartHomingBeam::kScatterCount; i++)
			{
				Vector3 scatterPos = lockTarget + beam.scatterOffsets[i];
				Vector3 to = scatterPos + (lockTarget - scatterPos) * easedT;

				float thick = 2.0f + easedT * 4.0f; // 収束が進むほど少し太くする
				float intensity = 0.4f + easedT * 0.6f;
				DrawBeamSegmentWithColor(cam, beamOrigin, to, thick, thick, color, intensity);
			}
		}
		else // Fired：発射済みなので1本の太いビームを描画
		{
			DrawJudggedBeam(cam, beamOrigin, beam.lockedPos, 15.0f, color, 1.0f);
		}
	}

	Renderer::SetBlendState(BS_NONE);
}
