#include "ChartObject.h"
#include "SimpleJson.h"
#include "Renderer.h"
#include "Camera.h"
#include "Input.h"
#include "Game.h"
#include "Application.h"
#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return a + (b - a) * t;
	}
	// レーン(0〜5) <-> S/D/F/J/K/Lキー
	constexpr int kLaneKeys[6] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };

	//ワールド座標をTexture2Dの座標系（画面中心が原点）に変換する
	Vector3 WorldToTexture2DPos(const Vector3& worldPos, const Matrix& view)
	{
		float screenW = (float)Application::GetWidth();
		float screenH = (float)Application::GetHeight();

		constexpr float fieldOfView = DirectX::XMConvertToRadians(45.0f);
		float aspectRatio = screenW / screenH;
		Matrix proj = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, aspectRatio, 1.0f, 1000.0f);

		DirectX::XMVECTOR projected = DirectX::XMVector3Project(
			DirectX::XMLoadFloat3(&worldPos),
			0.0f, 0.0f, screenW, screenH, 0.0f, 1.0f,
			proj, view, DirectX::XMMatrixIdentity()
		);

		Vector3 screenPos;
		DirectX::XMStoreFloat3(&screenPos, projected);
		// Texture2Dの2Dモードは画面中心が原点・Yが上向き正なので変換する
		return Vector3(screenPos.x - screenW * 0.5f, screenH * 0.5f - screenPos.y, 0.0f);
	}
}

Vector3 ChartJudgeCircle::GetPosition(float now) const
{
	if (keyframe.empty()) return Vector3::Zero;
	if (keyframe.size() == 1)
		return Vector3(keyframe[0].x, keyframe[0].y, keyframe[0].z);

	//nowより前後のキーフレームを探して補間
	if (now <= keyframe.front().time)
	{
		const auto& k = keyframe.front();
		return Vector3(k.x, k.y, k.z);
	}
	if (now >= keyframe.back().time)
	{
		const auto& k = keyframe.back();
		return Vector3(k.x, k.y, k.z);
	}

	for (size_t i = 0; i + 1 < keyframe.size(); i++)
	{
		const auto& a = keyframe[i];
		const auto& b = keyframe[i + 1];
		if (now >= a.time && now <= b.time)
		{
			float span = b.time - a.time;
			float t = (span > 0.00001f) ? (now - a.time) / span : 0.0f;
			Vector3 pa(a.x, a.y, a.z);
			Vector3 pb(b.x, b.y, b.z);
			return LerpVector3(pa, pb, t);
		}
	}
	const auto& k = keyframe.back();
	return Vector3(k.x, k.y, k.z);
}

void ChartObject::Init()
{
	StaticMesh staticmesh;

	std::string modelFile = "assets/model/note/music_sord_1.obj";
	std::string texDirectory = "assets/model/note";

	staticmesh.Load(modelFile, texDirectory);
	m_MeshRenderer.Init(staticmesh);

	m_Shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");

	m_Subsets = staticmesh.GetSubsets();
	m_Textures = staticmesh.GetTextures();

	std::vector<MATERIAL> materials = staticmesh.GetMaterials();
	for (size_t i = 0; i < materials.size(); i++)
	{
		std::unique_ptr<Material> m = std::make_unique<Material>();
		m->Create(materials[i]);
		m_Materials.push_back(std::move(m));
	}

	// デフォルトでは6レーン分の判定円を「動かない」位置で用意しておく
	// （LoadChartでjudgeCirclesがあれば上書きされる）
	m_JudgeCircles.resize(6);
	for (int i = 0; i < 6; i++)
	{
		m_JudgeCircles[i].lane = i;
		float laneX = -310.0f + (660.0f / 6.0f) * i + (660.0f / 12.0f);
		ChartCircleKeyframe key;
		key.time = 0.0f;
		key.x = -laneX;
		key.y = -55.0f;
		key.z = -210.0f;
		m_JudgeCircles[i].keyframe.push_back(key);
	}
	//接近時に表示されるテクスチャ
	m_ApproachRing.Init("assets/texture/ring.png");
	//パーティクルの画像セット
	m_ParticleQuad.Init("assets/texture/sakura2.png");

	//判定円アイコン
	const char* iconPaths[6] = {
		"assets/texture/S.png",
		"assets/texture/D.png",
		"assets/texture/F.png",
		"assets/texture/J.png",
		"assets/texture/K.png",
		"assets/texture/L.png",
	};
	float spacing = 90.0f;
	for (int i = 0; i < 6; i++)
	{
		m_LaneIcons[i].Init();
		m_LaneIcons[i].SetTexture(iconPaths[i]);
		m_LaneIcons[i].SetScale(70.0f, 70.0f, 1.0f);
	}

	//コンボ数表示
	for (int i = 0; i < 3; i++)
	{
		m_ComboDigits[i].Init();
		m_ComboDigits[i].SetTexture("assets/texture/number.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(40.0f, 60.0f, 1.0f);
	}

	//タイミングガイド：外枠と育つ四角
	const char* guideTexturePath_out = "assets/texture/sotohako.png";
	const char* guideTexturePath_in = "assets/texture/utihako.png";
	float frameSize = 100.0f;	//外枠のサイズ
	for (int i = 0; i < 6; i++)
	{
		m_LaneFrame[i].Init();
		m_LaneFrame[i].SetTexture(guideTexturePath_out);
		m_LaneFrame[i].SetScale(frameSize, frameSize, 1.0f);

		m_LaneFill[i].Init();
		m_LaneFill[i].SetTexture(guideTexturePath_in);
		m_LaneFill[i].SetScale(10.0f, 10.0f, 1.0f);
	}
}

//LoadChart : JSONから譜面読み込み
bool ChartObject::LoadChart(const std::string& name)
{
	JsonValue root;
	std::string err;
	if (!SimpleJson::ParseFile("charts/" + name + ".json", root, &err))
	{
		OutputDebugStringA(("[ChartObject] Load failed: " + err + "\n").c_str());
		return false;
	}
	m_Notes.clear();

	//ノーツ
	if (root.Has("notes") && root["notes"].IsArray())
	{
		for (const auto& n : root["notes"].arrayValue)
		{
			ChartNote note;
			note.lane = n["lane"].AsInt(0);
			note.type = n["type"].AsInt(0);
			note.time = n["time"].AsFloat(0.0f);

			if (n.Has("spawnTime"))
				note.spawnTime = n["spawnTime"].AsFloat(-1.0f);

			if (n.Has("spawnPos"))
			{
				const auto& sp = n["spawnPos"];
				note.spawnPos = Vector3(-sp["x"].AsFloat(), sp["y"].AsFloat(), sp["z"].AsFloat());
			}
			else
			{
				float laneX = -310.0f + (660.0f / 6.0f) * note.lane + (660.0f / 12.0f);
				note.spawnPos = Vector3(-laneX, -55.0f, 1000.0f);
			}

			if (n.Has("judgePos"))
			{
				const auto& jp = n["judgePos"];
				note.judgePos = Vector3(-jp["x"].AsFloat(), jp["y"].AsFloat(), jp["z"].AsFloat());
			}
			else
			{
				float laneX = -310.0f + (660.0f / 6.0f) * note.lane + (660.0f / 12.0f);
				note.judgePos = Vector3(-laneX, -55.0f, -210.0f);
			}

			if (n.Has("path") && n["path"].IsArray())
			{
				for (const auto& p : n["path"].arrayValue)
				{
					note.controlPoints.emplace_back(
						-p["x"].AsFloat(), p["y"].AsFloat(), p["z"].AsFloat()
					);
				}
			}
			m_Notes.push_back(note);
		}
		std::sort(m_Notes.begin(), m_Notes.end(),
			[](const ChartNote& a, const ChartNote& b) {return a.time < b.time; });
	}

	//判定円のキーフレーム
	if (root.Has("judgeCircles") && root["judgeCircles"].IsArray())
	{
		for (const auto& jc : root["judgeCircles"].arrayValue)
		{
			int lane = jc["lane"].AsInt(-1);
			if (lane < 0 || lane >= (int)m_JudgeCircles.size()) continue;

			auto& circle = m_JudgeCircles[lane];
			circle.keyframe.clear();

			if (jc.Has("keyframe") && jc["keyframe"].IsArray())
			{
				for (const auto& k : jc["keyframe"].arrayValue)
				{
					ChartCircleKeyframe key;
					key.time = k["time"].AsFloat();
					key.x = -k["x"].AsFloat();
					key.y = k["y"].AsFloat();
					key.z = k["z"].AsFloat();
					circle.keyframe.push_back(key);
				}
			}
			std::sort(circle.keyframe.begin(), circle.keyframe.end(),
				[](const ChartCircleKeyframe& a, const ChartCircleKeyframe& b) {
					return a.time < b.time;
				});
		}
	}
	//再生開始状態にリセット
	m_Started = false;
	m_MusicTime = 0.0f;
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;

	OutputDebugStringA(("[ChartPlayObject] Loaded: charts/" + name + ".json notes=" +
		std::to_string(m_Notes.size()) + "\n").c_str());
	return true;
}

//EvaluateNotePosition : ベジェ補間
Vector3 ChartObject::EvaluateNotePosition(const ChartNote& note, float now) const
{
	float startTime = note.GetSpawnTime();
	float endTime = note.time;
	float duration = endTime - startTime;
	if (duration < 0.0001f) duration = 0.0001f;

	float t = (now - startTime) / duration;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	const auto& p0 = note.spawnPos;
	const auto& p3 = note.judgePos;

	if (note.controlPoints.empty())
	{
		return LerpVector3(p0, p3, t);
	}

	if (note.controlPoints.size() == 1)
	{
		const auto& p1 = note.controlPoints[0];
		return LerpVector3(LerpVector3(p0, p1, t), LerpVector3(p1, p3, t), t);
	}
	const auto& p1 = note.controlPoints[0];
	const auto& p2 = note.controlPoints[1];
	auto a = LerpVector3(p0, p1, t);
	auto b = LerpVector3(p1, p2, t);
	auto c = LerpVector3(p2, p3, t);
	return LerpVector3(LerpVector3(a, b, t), LerpVector3(b, c, t), t);
}

//Update
void ChartObject::Update()
{
	if (!m_Started)
	{
		m_StartClock = std::chrono::steady_clock::now();
		m_LastUpdateClock = m_StartClock;
		m_Started = true;
	}
	auto nowClock = std::chrono::steady_clock::now();
	m_MusicTime = std::chrono::duration<float>(nowClock - m_StartClock).count();

	float deltaTime = std::chrono::duration<float>(nowClock - m_LastUpdateClock).count();
	m_LastUpdateClock = nowClock;

	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

	// 判定円をキーフレームアニメーションに合わせて動かし、
	// まだ判定されていないノーツの judgePos を追従させる
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (note.lane < 0 || note.lane >= (int)m_JudgeCircles.size()) continue;
		note.judgePos = m_JudgeCircles[note.lane].GetPosition(m_MusicTime);
	}

	for (auto it = m_Particle.begin(); it != m_Particle.end();)
	{
		it->Update(deltaTime);

		if (it->IsStop())
			it = m_Particle.erase(it);
		else
			++it;
	}

	//ミス判定
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (m_MusicTime - note.time > kGud)
		{
			note.judged = true;
			m_Miss++;
		}
	}

	// S/D/F/J/K/Lキーで判定
	for (int lane = 0; lane < 6; lane++)
	{
		JudgeLane(kLaneKeys[lane], lane);
	}
}

void ChartObject::JudgeLane(int key, int laneIndex)
{
	if (!Input::GetKeyTrigger(key)) return;

	int bestIndex = -1;
	float bestDiff = 9999.0f;

	for (int i = 0; i < (int)m_Notes.size(); i++)
	{
		if (m_Notes[i].judged) continue;
		if (m_Notes[i].lane != laneIndex) continue;

		float diff = std::fabs(m_Notes[i].time - m_MusicTime);
		if (diff < bestDiff) { bestDiff = diff; bestIndex = i; }
	}
	if (bestIndex == -1) return;
	if (bestDiff > kGud) return;	//判定圏外

	int rank = (bestDiff < kPerfect) ? 0 : (bestDiff < kGreat) ? 1 : 2;
	if (rank == 0) m_Perfect++;
	else if (rank == 1) m_Great++;
	else if (rank == 2) m_Good++;

	m_Notes[bestIndex].judged = true;
	OnJudge(laneIndex, rank);
}

void ChartObject::OnJudge(int laneIndex, int rank)
{
	OutputDebugStringA("[Particle] OnJudge called\n");
	m_Combo++;
	m_ComboPopTimer = 0.15f;
	// TODO: 判定エフェクト（回転する四角バースト）をここに実装する。
	// VertexBuffer/IndexBufferのAPIが分かり次第、動的頂点バッファで
	// Texture2Dと同じ要領のクワッドを複数枚描画する形にする予定。

	Vector3 pos = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	//あとでパーティクルの数は変数化するかも
	for (int i = 0; i < 100; i++)
	{
		float deg = rand() % 360;

		Particle p(
			pos,
			1.5f,                         // 寿命
			50.0f,                        // 重力を弱める
			Color(1, 1, 1, 1),
			Color(1, 1, 1, 0),
			0.7f,                         // 終盤30%だけフェード
			80.0f,                        // 開始サイズ:80
			40.0f,                        // 終了サイズ
			Vector3(80, 120, 80),         // 速度を弱める
			deg
		);
		m_Particle.push_back(p);
	}

	(void)laneIndex;
	(void)rank;
}

void ChartObject::Draw(Camera* cam)
{
	// 3Dカメラを再アサート（他オブジェクトがTexture2Dで2Dに切り替えている場合があるため）
	cam->SetCamera(0);

	char buf[64];
	//std::cout << "Particle Count = "
	//	<< m_Particle.size()
	//	<< std::endl;

	Renderer::SetBlendState(BS_NONE);

	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();

	for (auto& note : m_Notes)
	{
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		float et = note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		Vector3 pos = EvaluateNotePosition(note, m_MusicTime);

		Matrix s = Matrix::CreateScale(m_NoteScale);
		Matrix t = Matrix::CreateTranslation(pos);
		Matrix world = s * t;
		Renderer::SetWorldMatrix(&world);

		for (size_t i = 0; i < m_Subsets.size(); i++)
		{
			m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();
			if (m_Materials[m_Subsets[i].MaterialIdx]->isTextureEnable())
				m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();

			m_MeshRenderer.DrawSubset(
				m_Subsets[i].IndexNum,
				m_Subsets[i].IndexBase,
				m_Subsets[i].VertexBase
			);
		}
	}
	//接近リング　判定ラインに近づくにつれて大きく濃くなる
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		float et = note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		float duration = et - st;
		float t = (duration > 0.0001f) ? (m_MusicTime - st) / duration : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		Vector3 pos = EvaluateNotePosition(note, m_MusicTime);

		float ringScale = 100.0f + t * 60.0f;
		float ringAlpha = 0.2f + t * 0.6f;
		m_ApproachRing.Draw(pos, ringScale, ringAlpha);
	}
	Renderer::SetBlendState(BS_NONE);

	//レーンごとに一番近い未判定ノーツの接近度(0〜1)を計算
	float laneT[6] = { 0,0,0,0,0,0 };
	bool laneActive[6] = { false,false,false,false,false,false };
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;	//ミスが確定してたらスルー
		if (note.lane < 0 || note.lane >= 6) continue;

		float st = note.GetSpawnTime();
		float et = note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;	// 出現〜判定の範囲外はスキップ

		float duration = et - st;
		float t = (duration > 0.00001f) ? (m_MusicTime - st) / duration : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		//そのレーンでもっとも判定に近いノーツを選択
		if (!laneActive[note.lane] || t > laneT[note.lane])
		{
			laneActive[note.lane] = true;
			laneT[note.lane] = t;
		}
	}

	//判定円アイコン：judgeCirclesの3D座標を画面に投影して表示
	const Matrix view = cam->GetViewMatrix();
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		Vector3 circlePos3D = m_JudgeCircles[i].GetPosition(m_MusicTime);
		Vector3 screenPos = WorldToTexture2DPos(circlePos3D, view);

		m_LaneIcons[i].SetPosition(screenPos);
		m_LaneIcons[i].Draw(cam);

		Renderer::SetBlendState(BS_ALPHABLEND);
		m_LaneFrame[i].SetPosition(screenPos);
		m_LaneFrame[i].Draw(cam);

		if (laneActive[i])
		{
			float fillSize = 10.0f + laneT[i] * 90.0f;
			m_LaneFill[i].SetScale(fillSize, fillSize, 1.0f);
			m_LaneFill[i].SetPosition(screenPos);
			m_LaneFill[i].Draw(cam);
		}
	}
	Renderer::SetBlendState(BS_NONE);

	//パーティクル
	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (auto& p : m_Particle)
	{
		m_ParticleQuad.Draw(
			cam,
			p.GetPosition(),
			p.GetScale(),
			p.GetColor()
		);
	}
	Renderer::SetBlendState(BS_NONE);

	//コンボ数表示
	{
		int combo = m_Combo;
		if (combo > 999) combo = 999;

		int digits[3] = { combo / 100,(combo / 10) % 10, combo % 10 };
		// ヒット直後は少し拡大してから元に戻る「ポップ」演出
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 40.0f;
		float startX = 200.0f;	//画面右寄りに配置してます

		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 3; i++)
		{
			m_ComboDigits[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
			m_ComboDigits[i].SetScale(40.0f * popScale, 60.0f * popScale, 1.0f);
			m_ComboDigits[i].SetPosition(startX + digitW * i, 250.0f, 0.0f);
			m_ComboDigits[i].Draw(cam);
		}
	}

	cam->SetCamera(0);
}

void ChartObject::Uninit()
{

}
