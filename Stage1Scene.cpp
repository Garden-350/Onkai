#include "Stage1Scene.h"
#include "Game.h"
#include "Input.h"
#include "ChartObject.h"
#include "SkyBox.h"
#include "Enemy.h"
#include "Player.h"
#include "Ground.h"
#include "WireShape.h"
#include "WireShapeSpawner.h"
#include "StageBackdrop.h"
#include "SpotlightRow.h"
#include "OuterEdge.h"
#include "PauseOverlay.h"
#include "CameraLayerMarker.h"

using namespace DirectX::SimpleMath;



// コンストラクタ
Stage1Scene::Stage1Scene()
{
	Init();
}

// デストラクタ
Stage1Scene::~Stage1Scene()
{
	Uninit();
}



// 初期化
void Stage1Scene::Init()
{
	// 横からの斜め見下ろし視点：
	// プレイヤーの斜め後方・少し上から、奥の敵方向を見下ろす
	//Game::GetInstance()->GetCamera()->SetFixedPose(
	//	Vector3(0.0f, 667.0f, -850.0f),   // カメラ位置：プレイヤーのやや上・後方
	//	Vector3(0.0f, 0.0f, -300.0f)      // 注視点：奥(敵方向)の少し高い位置
	//);

	//auto* ground = Game::GetInstance()->AddObject<Ground>();
	//m_MySceneObjects.emplace_back(ground);
	//auto* skybox = Game::GetInstance()->AddObject<SkyBox>();
	//m_MySceneObjects.emplace_back(skybox);
	//auto* worldLayer = Game::GetInstance()->AddObject<CameraLayerMarker>();
	//worldLayer->SetUp(Camera::Layer::World, false);
	//m_MySceneObjects.emplace_back(worldLayer);

	//背景画像
	auto* backdrop = Game::GetInstance()->AddObject<StageBackdrop>();
	m_MySceneObjects.emplace_back(backdrop);
	// ランウェイ風スポットライト（背景の次、敵やノーツより前に追加する）
	auto* spotlights = Game::GetInstance()->AddObject<SpotlightRow>();
	m_MySceneObjects.emplace_back(spotlights);
	m_Spotlights = spotlights;
	auto* outeredge = Game::GetInstance()->AddObject<OuterEdge>();
	m_MySceneObjects.emplace_back(outeredge);

	auto* enemy = Game::GetInstance()->AddObject<Enemy>();
	m_MySceneObjects.emplace_back(enemy);
	spotlights->SetFarTarget(enemy->GetPosition());	//列の奥の端を敵の位置に設定
	auto* player = Game::GetInstance()->AddObject<Player>();
	m_MySceneObjects.emplace_back(player);
	auto* chart = Game::GetInstance()->AddObject<ChartObject>();
	chart->LoadChart(Game::GetInstance()->GetSelectedChart());
	m_MySceneObjects.emplace_back(chart);
	m_Chart = chart;
	ApplyCamera();
	chart->SetEnemyRef(enemy);
	chart->SetPlayerRef(player);
	//横方向の正方形グリッド表示
	WireShapeSpawner* spaen = Game::GetInstance()->AddObject<WireShapeSpawner>();
	spaen->SetSceneObjects(&m_MySceneObjects);
	m_MySceneObjects.emplace_back(spaen);

	auto* pause = Game::GetInstance()->AddObject<PauseOverlay>();
	m_MySceneObjects.emplace_back(pause);

	Game::GetInstance()->GetCamera()->SetWorldOffset(0.0f,
		DirectX::XMConvertToRadians(0.0f), 0.0f);	//確認用
}



//更新
void Stage1Scene::Update()
{
	// エンターキーを押してタイトルへ
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		SendResult();
		return;
	}

	if (!m_Chart)return;

#ifdef _DEBUG
	{
		bool changed = false;
		const float step = 10.0f;
		if (Input::GetKeyPress(VK_F1)) { m_CamHeight -= step; changed = true; }
		if (Input::GetKeyPress(VK_F2)) { m_CamHeight += step; changed = true; }
		if (Input::GetKeyPress(VK_F3)) { m_CamBack -= step; changed = true; }
		if (Input::GetKeyPress(VK_F4)) { m_CamBack += step; changed = true; }
		if (Input::GetKeyPress(VK_F5)) { m_CamLookAhead -= step; changed = true; }
		if (Input::GetKeyPress(VK_F6)) { m_CamLookAhead += step; changed = true; }
		if (changed)
		{
			ApplyCamera();
			char buf[128];
			sprintf_s(buf, "[Camera] height=%.0f back=%.0f lookAhead=%.0f\n", m_CamHeight, m_CamBack, m_CamLookAhead);
			OutputDebugStringA(buf);
		}
	}
#endif


	//スポットライトを曲の拍に合わせる
	if (m_Spotlights) m_Spotlights->SetBeat(m_Chart->GetMusicTime(), m_Chart->GetBpm());

	auto now = std::chrono::steady_clock::now();
	if (!m_ClockInitialized)
	{
		m_LastUpdateClock = now;
		m_ClockInitialized = true;
	}
	float dt = std::chrono::duration<float>(now - m_LastUpdateClock).count();
	m_LastUpdateClock = now;

	if (!m_AudioFinished && m_Chart->IsAudioFinish())
	{
		m_AudioFinished = true;
		m_FinishWaitTimer = 0.0f;
	}
	if (m_AudioFinished)
	{
		m_FinishWaitTimer += dt;
		if (m_FinishWaitTimer >= kResultDelaySeconds)
		{
			SendResult();
		}
	}
}

// 終了処理
void Stage1Scene::Uninit()
{
	// このシーンのオブジェクトを削除する
	for (auto& o : m_MySceneObjects) {
		Game::GetInstance()->DeleteObject(o);
	}
	m_MySceneObjects.clear();
	Game::GetInstance()->GetCamera()->SetWorldOffset(0.0f, 0.0f, 0.0f);
}

void Stage1Scene::SendResult()
{
	if (m_Chart)
	{
		ResultData result;
		result.perfect = m_Chart->GetPerfectCount();
		result.great = m_Chart->GetGreatCount();
		result.good = m_Chart->GetGoodCount();
		result.miss = m_Chart->GetMissCount();
		result.maxCombo = m_Chart->GetMaxComboCount();
		result.totalNotes = m_Chart->GetTotalNoteCount();
		result.isFullCombo = m_Chart->IsFullCombo();
		result.chartName = Game::GetInstance()->GetSelectedChart();
		Game::SetResultData(result);
	}
	Game::GetInstance()->ChangeScene(RESULT);
}

void Stage1Scene::ApplyCamera()
{
	if (!m_Chart) return;
	Vector3 c = m_Chart->GetJudgeLineCenter();
	Vector3 eye = kUseFixedEye ? m_FixedEye : c + Vector3(0.0f, m_CamHeight, -m_CamBack);   // カメラの位置
	Vector3 target = c + Vector3(0.0f, 0.0f, m_CamLookAhead);      // 見る先
	Game::GetInstance()->GetCamera()->SetFixedPose(eye, target);

	char buf[200];
	sprintf_s(buf, "[Camera] height=%.0f back=%.0f lookAhead=%.0f | eye=(%.0f, %.0f, %.0f) target=(%.0f, %.0f, %.0f)\n",
		m_CamHeight, m_CamBack, m_CamLookAhead, eye.x, eye.y, eye.z, target.x, target.y, target.z);
	OutputDebugStringA(buf);
}

