#include "ChartObject.h"
#include "ModelUtil.h"
#include "NotePath.h"
#include "ChartLoader.h"
#include "SimpleJson.h"
#include "Renderer.h"
#include "Camera.h"
#include "Input.h"
#include "Game.h"
#include "Player.h"
#include "Application.h"
#include "sound.h"
#include "Enemy.h"
#include "LightShaft.h"
#include "ResultData.h"
#include "ChargeAura.h"
#include "ScoreRule.h"
#include "NoteBehavior.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

// ============================================================
// 無名namespace：このファイル内だけで使う小道具関数
// ============================================================
namespace
{
	// 2つのVector3を線形補間する
	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return a + (b - a) * t;
	}

	// レーン(0〜5) <-> S/D/F/J/K/Lキーの対応表
	constexpr int kLaneKeys[6] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };

	//ロングノーツが持つチェックポイントを数える
	int CountLongNoteCheckPoint(float arrivalTime, float endTime)
	{
		constexpr float kEps = 0.0001f;
		float t = arrivalTime;
		int count = 0;
		while (true)
		{
			bool isFinal = (t >= endTime - kEps);
			count++;
			if (isFinal) break;
			t += JudgeSystem::kLongTickInterval;
			if (t > endTime - kEps) t = endTime; // 最後のtickは必ずendTimeに合わせる
		}
		return count;
	}

	// ワールド座標を、画面固定2D描画(Texture2D)が使う座標系（画面中心=原点）に変換する
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
		// スクリーン座標(左上原点)→Texture2D座標系(中心原点、Y上向き正)へ変換
		return Vector3(screenPos.x - screenW * 0.5f, screenH * 0.5f - screenPos.y, 0.0f);
	}
}

// ============================================================
// ChartJudgeCircle::GetPosition
//   now(秒)時点での判定円の座標を、前後のキーフレーム間で線形補間して返す。
//   キーフレームが1個しかない場合は常にその座標を返す（=動かない判定円）。
// ============================================================
Vector3 ChartJudgeCircle::GetPosition(float now) const
{
	if (keyframe.empty()) return Vector3::Zero;
	if (keyframe.size() == 1)
		return Vector3(keyframe[0].x, keyframe[0].y, keyframe[0].z);

	// nowが最初のキーフレームより前 → 先頭の座標に固定
	if (now <= keyframe.front().time)
	{
		const auto& k = keyframe.front();
		return Vector3(k.x, k.y, k.z);
	}
	// nowが最後のキーフレームより後 → 末尾の座標に固定
	if (now >= keyframe.back().time)
	{
		const auto& k = keyframe.back();
		return Vector3(k.x, k.y, k.z);
	}

	// nowを挟む2つのキーフレームを探して線形補間
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
	// ここに来ることは理論上ないが、保険として末尾を返す
	const auto& k = keyframe.back();
	return Vector3(k.x, k.y, k.z);
}

// LoadOptionalModel / DrawModel は ModelUtil.h/.cpp に切り出し済み

DirectX::SimpleMath::Vector3 ChartObject::GetJudgeLineCenter() const
{
	if (m_JudgeCircles.empty()) return Vector3::Zero;
	Vector3 sum = Vector3::Zero;
	for (const auto& c : m_JudgeCircles) sum += c.GetPosition(0.0f);

	return sum / (float)m_JudgeCircles.size();
}

// ============================================================
// Init : オブジェクト初期化。
//   各種3Dモデル・テクスチャ・判定円のデフォルト値をセットアップする。
//   実際の譜面データは LoadChart で別途読み込む。
// ============================================================
void ChartObject::Init()
{
	NoteRegistry::RegisterAll();	//新しい種類のノーツを登録

	m_AutoMode = Game::GetInstance()->IsAutoMode();
	m_NoiseMode = Game::GetInstance()->IsNoiseMode();
	m_JudgeSystem.SetOnJudge([this](int laneIndex, int rank) { OnJudge(laneIndex, rank); });
	m_JudgeSystem.SetOnChargeHit([this](int rank) { m_Special.AddHit(rank, true); });
	static bool s_AuraInited = false;
	if (!s_AuraInited)
	{
		ChargeAura::Instance().Init();
		s_AuraInited = true;
	}
	m_Special.SetOnChargeStart([this]()
		{
			ChargeAura::Instance().Start();
		});
	m_Special.SetOnFire([this](float ratio)
		{
			ChargeAura::Instance().Release();
			if (!m_EnemyRef) return;

			//オートプレイは必ず弱点を狙う
			if (m_AutoMode) m_WeakPoint.SetAim(m_WeakPoint.GetWeakLane());

			int aim = m_WeakPoint.GetAim();
			bool critical = (aim == m_WeakPoint.GetWeakLane());

			constexpr float kFromUp = 40.0f;	//プレイヤーのどれだけ上から撃つか
			constexpr float kToUp = 50.0f;		//敵の中心のどれだけ上に当てるか
			Vector3 from = m_PlayerRef ? m_PlayerRef->GetPosition() + Vector3(0.0f, kFromUp, 0.0f)
				: Vector3(0.0f, 0.0f, -300.0f);
			Vector3 to = m_EnemyRef->GetPosition() + Vector3(0.0f, kToUp, 0.0f);	//ビームは必ず敵のど真ん中へ

			m_WeakPoint.OnFired();
			m_SpecialBeam.Fire(from, to, ratio, critical);	//ダメージは当たった瞬間（SetOnHit）
			sound.Play(SOUND_BEAM_FIRING);
		});
	//発射が終わった：弱点を普段の切り替えに戻す
	m_Special.SetOnFinish([this]()
		{
			m_WeakPoint.EndSpecial(m_MusicTime);
		});

	m_JudgeSystem.SetOnLongEnd([this](int)
		{
			m_GazeKickTime = m_MusicTime;
			//カウンター　HPの4割りをロングの本数で割った分のダメージにする
			if (m_EnemyRef && m_LongCount > 0) m_EnemyRef->Damage(40.0f / m_LongCount);
			if (m_EnemyRef) m_EnemyRef->Flash(Vector3(1.0f, 1.0f, 1.0f), 0.6f, 0.15f);
		});
	m_JudgeSystem.SetOnEmptyPress([this](int laneIndex) { OnEmptyLanePress(laneIndex); });
	// サウンドエンジン初期化 
	sound.Init();

	//  TAPノーツ本体モデルのロード（これは必須。無いと何も表示されない）
	StaticMesh staticmesh;
	std::string modelFile = "assets/model/note/tap_3.fbx";
	std::string texDirectory = "assets/model/maru";

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

	//タップのテクスチャをコードで貼る
	ModelUtil::ApplyTexture(m_Materials, m_Textures, "assets/model/note/note_tap.png");

	// ── ロングノーツ：先端(sentan)モデルのロード ──
	// エディター側の kusari_sentan_2.fbx に相当するアセットを想定。
	// 無ければ m_LongSentanLoaded=false のままとなり、描画側でTAPモデルにフォールバックする。
	m_LongSentanLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/long/sentan_4.fbx", "assets/model/sentan",
		m_LongSentanRenderer, m_LongSentanShader, m_LongSentanSubsets, m_LongSentanMaterials, m_LongSentanTextures);
	if (!m_LongSentanLoaded)
		OutputDebugStringA("[ChartObject] Long sentan model not found. Fallback to TAP model.\n");

	// ── ロングノーツ：本体(dou/鎖)モデルのロード ──
	// エディター側の kusari_dou.fbx に相当。押している間、先端〜終端の間に繰り返し並べる。
	m_LongDouLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/long/dou_4.fbx", "assets/model/dou_2",
		m_LongDouRenderer, m_LongDouShader, m_LongDouSubsets, m_LongDouMaterials, m_LongDouTextures);
	if (!m_LongDouLoaded)
		OutputDebugStringA("[ChartObject] Long dou model not found. Fallback to TAP model.\n");
	if(!m_LongDouGlowTexture.Load("assets/texture/dou_glow.png"))
		OutputDebugStringA("[ChartObject] dou glow texture not found.\n");
	m_LongDouGlowShader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

	// ── ロングノーツ：終端(o/輪)モデルのロード ──
	// エディター側の kusari_o_2.fbx に相当。先端よりholdDuration秒遅れて追従する。
	m_LongOLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/long/syuutan_4.fbx", "assets/model/syuutan_3",
		m_LongORenderer, m_LongOShader, m_LongOSubsets, m_LongOMaterials, m_LongOTextures);
	if (!m_LongOLoaded)
		OutputDebugStringA("[ChartObject] Long o model not found. Fallback to TAP model.\n");

	// ── フリック用モデルのロード ──
	m_FlickModelLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/Flick/allow.fbx", "assets/model/flick",
		m_FlickMeshRenderer, m_FlickShader, m_FlickSubsets, m_FlickMaterials, m_FlickTextures);
	if (!m_FlickModelLoaded)
		OutputDebugStringA("[ChartObject] Flick model not found. Fallback to TAP model.\n");

	// ── ビーム用モデルの初期化は HomingBeamSystem::Init に切り出し済み ──
	m_BeamSystem.Init();
	m_LongBeam.Init();

	//必殺技のビームが当たったりしたところ
	m_SpecialBeam.Init();
	m_WeakPoint.Init();
	for (int i = 0; i < 6; i++) m_WeakPoint.SetLaneColor(i, GetLaneColor(i));
	m_AttackOrbs.Init();
	m_ImpactFx.Init();

	//敵が張るシールド
	m_LongShield.Init();
	m_LongShield.SetOnCrack([this](int, int)
		{
			sound.Play(SOUND_MIRROR_BREAK_SMALL);	//ちょっと割れる音
		});
	//盾が割れた：ロングの分のダメージと、割れた演出
	m_LongShield.SetOnBreak([this](int longCount, const Vector3& center)
		{
			if (m_EnemyRef && m_LongCount > 0) m_EnemyRef->Damage(40.0f / m_LongCount * longCount);
			if (m_EnemyRef) m_EnemyRef->Flash(Vector3(0.6f, 0.85f, 1.0f), 0.8f, 0.25f);
			m_ImpactFx.SpawnCritical(center, Vector3(0.45f, 0.75f, 1.0f), 0.6f);
			m_ScreenEffects.TriggerScreenShake(0.25f, 8.0f);
			m_ScreenEffects.TriggerFlash(0.12f);
			m_GazeKickTime = m_MusicTime;	//世界側のカメラを一瞬見上げる（今まで押し切りでやっていたもの）
			sound.Play(SOUND_MIRROR_BREAK_BIG);	//割れた音
		});


	//必殺技の本命の帯が当たった瞬間
	m_SpecialBeam.SetOnHit([this](float ratio, bool critical, const Vector3& hitPos)
		{
			float damage = SpecialMove::CalcDamage(kSpecialBaseDamage, ratio)
				* (critical ? kSpecialCritMul : kSpecialBlockedMul);
			if (m_EnemyRef) m_EnemyRef->Damage(damage);

			if (critical)
			{
				//弱点に当たった：大きな衝撃・閃光・強い揺れ・世界側カメラのキック
				m_ImpactFx.SpawnCritical(hitPos, Vector3(1.0f, 0.85f, 0.4f), ratio);
				m_ScreenEffects.TriggerScreenShake(0.35f, 8.0f + 10.0f * ratio);
				m_ScreenEffects.TriggerFlash(0.18f);
				if (m_EnemyRef) m_EnemyRef->Flash(Vector3(1.0f, 0.9f, 0.5f), 1.0f, 0.35f);	//金色に強く
				m_GazeKickTime = m_MusicTime;
				sound.Play(SOUND_METAL_2);
			}
			else
			{
				//防がれた：盾と、はね返る火花 盾がいるかは要検討
				Vector3 from = m_PlayerRef ? m_PlayerRef->GetPosition() : Vector3(0.0f, 0.0f, -300.0f);
				m_ImpactFx.SpawnBlocked(hitPos, hitPos - from);
				m_ScreenEffects.TriggerScreenShake(0.15f, 4.0f);
				if (m_EnemyRef) m_EnemyRef->Flash(Vector3(0.55f, 0.75f, 1.0f), 0.6f, 0.2f);	//青白く（盾の色があったら）
				sound.Play(SOUND_REFLECTION);
			}
		});
	//必殺技の群れの光が1本当たるたび
	m_SpecialBeam.SetOnSubHit([this](const Vector3& pos, const Vector3& color)
		{
			m_ImpactFx.SpawnSmallHit(pos, color);
		});
	//普段の攻撃の光が着いた時
	m_AttackOrbs.SetOnArrive([this](int, const Vector3& pos, const Vector3& color, bool critical)
		{
			if (m_LongShield.IsUp())
			{
				m_LongShield.Ripple(pos);	// 盾にはじかれた
				return;
			}

			if (critical) m_ImpactFx.SpawnWeakHit(pos, color);
			else          m_ImpactFx.SpawnSmallHit(pos, color);

			//敵を一瞬光らせる
			if (m_EnemyRef)
			{
				if (critical) m_EnemyRef->Flash(Vector3(1.0f, 0.85f, 0.4f), 0.45f, 0.12f);
				else          m_EnemyRef->Flash(Vector3(1.0f, 1.0f, 1.0f), 0.2f, 0.08f);
			}
		});

	// ── 判定円の初期配置（JSONにjudgeCirclesが無い旧フォーマット用のデフォルト値）──
// ── 判定円の初期配置（JSONにjudgeCirclesが無い旧フォーマット用のデフォルト値）──
	m_JudgeCircles.resize(6);
	// ChartObject::Init 内
	for (int i = 0; i < 6; i++)
	{
		m_JudgeCircles[i].lane = i;
		float laneX = -50.0f + (1660.0f / 6.0f) * i + (1660.0f / 12.0f);
		ChartCircleKeyframe key;
		key.time = 0.0f;
		key.x = -laneX;
		key.y = 1050.0f;     // -55→-20 (プレイヤーとのY差を少し詰める)
		key.z = 300.0f;     // 150→300 (カメラから適度に離す)
		m_JudgeCircles[i].keyframe.push_back(key);
	}

	// ── 接近リング・パーティクルの画像セット ──
	m_ApproachRing.Init("assets/texture/ring.png");
	m_ParticleQuad.Init("assets/texture/sakura2.png");
	m_LongParticleQuad.Init("assets/texture/beamP.png");
	m_MagicCircleQuad.Init("assets/texture/magic-circle (1).png");
	m_TapAuraParticleQuad.Init("assets/texture/oura.png");

	// ── レーンアイコン(S/D/F/J/K/L)のセットアップ ──
	const char* iconPaths[6] = {
		"assets/texture/S.png", "assets/texture/D.png", "assets/texture/F.png",
		"assets/texture/J.png", "assets/texture/K.png", "assets/texture/L.png",
	};
	for (int i = 0; i < 6; i++)
	{
		m_LaneIcons[i].Init();
		m_LaneIcons[i].SetTexture(iconPaths[i]);
		m_LaneIcons[i].SetScale(120.0f, 120.0f, 1.0f);
	}

	m_ClosingRingGuide.Init();
	m_HitPillar.Init();
	m_JudgeGuide.Init();

	//ロングノーツを押してる間だすテクスチャ
	m_BarrierPlate.Init("assets/texture/barrier.png");

	// ── タイミングガイド：外枠と育つ四角のセットアップ ──
	const char* guideTexturePath_out = "assets/texture/sotohako.png";
	const char* guideTexturePath_in = "assets/texture/utihako.png";
	for (int i = 0; i < 6; i++)
	{
		m_LaneFrame[i].Init();
		m_LaneFrame[i].SetTexture(guideTexturePath_out);
		m_LaneFrame[i].SetScale(m_FrameSize, m_FrameSize, 1.0f);

		m_LaneFill[i].Init();
		m_LaneFill[i].SetTexture(guideTexturePath_in);
		m_LaneFill[i].SetScale(m_FrameSize / 10, m_FrameSize / 10, 1.0f);
	}
	m_ComboHud.Init();

	m_keyt.Init();

	m_ScoreDisplay.Init();
	m_ScoreDisplay.SetUp("assets/texture/allnumber_new.png", 7);
	m_ScoreDisplay.SetDigitSize(72.0f, 72.0f);
	m_ScoreDisplay.SetPosition(-400.0f, 325.0f);
	m_ScoreDisplay.SetLeadingZeroAlpha(0.25f);	//頭の0を薄くする

	m_EnemyHp.Init();
	m_EnemyHp.SetPosition(400.0f, 230.0f);	//画面の上
	m_EnemyHp.SetSize(250.0f);

	m_SpGauge.Init();
	m_SpGauge.SetPosition(-540.0f, -100.0f);
}

// ============================================================
// LoadChart : charts/<name>.json を読み込み、譜面データ一式をセットする。
//   JSONの座標系はエディター側と符号が逆(X軸反転)なので、
//   spawnPos/judgePos/path/judgeCirclesのxだけマイナスを掛けて読み込む。
//
//   各フィールドは、旧フォーマット(項目が無いJSON)でも動くように
//   Has()チェックのうえデフォルト値を補完している。
// ============================================================
bool ChartObject::LoadChart(const std::string& name)
{
	ChartLoadResult result = ChartLoader::Load(name, m_JudgeCircles);
	if (!result.success)
		return false;

	m_Notes = result.notes;
	m_TotalNote = 0;
	m_LongCount = 0;
	for (const auto& note : m_Notes)
	{
		if (note.IsLong())
		{
			m_TotalNote += CountLongNoteCheckPoint(note.arrivalTime, note.time);
			m_LongCount++;
		}
		else
		{
			m_TotalNote++;
		}
	}
	m_JudgeCircles = result.judgeCircles;

	if (kHighwayMode)
	{
		for (auto& note : m_Notes)
		{
			if (note.IsFlick()) continue;	//フリックは敵側へ飛ぶので今まで通り
			if (note.lane < 0 || note.lane >= (int)m_JudgeCircles.size()) continue;

			float arrive = note.IsLong() ? note.arrivalTime : note.time;	//判定んに届く時間
			Vector3 judge = m_JudgeCircles[note.lane].GetPosition(arrive);
			note.spawnPos = judge + Vector3(0.0f, 0.0f, m_HighwayDepth);	//同じ高さで奥から
			note.spawnTime = arrive - m_HighwayApproach;	//全ノーツ同じ速さで
			note.controlPoints.clear();	//曲線じゃなくて直線で
			note.spiralRadius = 0.0f;	//螺旋もやめる
		}
	}

	//判定ガイドのトラック：レーンごとに「ノーツがどっちから・何秒かけて来るか」の平均を取っておく
	{
		Vector3 sum[6];
		int count[6] = { 0,0,0,0,0,0 };
		Vector3 allSum = Vector3::Zero;
		int allCount = 0;
		double approachSum = 0.0;
		int approachCount = 0;
		for (const auto& note : m_Notes)
		{
			if (note.IsFlick()) continue;	// フリックは敵側へ飛ぶので対象外
			if (note.lane < 0 || note.lane >= 6 || note.lane >= (int)m_JudgeCircles.size()) continue;

			float arrive = note.IsLong() ? note.arrivalTime : note.time;
			Vector3 judgeAtHit = m_JudgeCircles[note.lane].GetPosition(arrive);	// そのノーツが届く瞬間の判定円
			Vector3 offset = note.spawnPos - judgeAtHit;
			sum[note.lane] += offset;
			count[note.lane]++;
			allSum += offset;
			allCount++;

			float duration = arrive - note.GetSpawnTime();
			if (duration > 0.05f)
			{
				approachSum += duration;
				approachCount++;
			}
		}
		Vector3 fallback = (allCount > 0) ? allSum / (float)allCount : Vector3(0.0f, 205.0f, 630.0f);
		for (int i = 0; i < 6; i++)
			m_LaneTrackOffset[i] = (count[i] > 0) ? sum[i] / (float)count[i] : fallback;
		for (int i = 0; i < 6; i++) m_LaneTrackOffset[i].x = 0.0f;	// 横方向は捨てて、真っすぐ奥へ伸ばす
		m_TrackApproachSeconds = (approachCount > 0) ? (float)(approachSum / approachCount) : 1.0f;
	}

	m_BeamSystem.SetBeams(result.homingBeams);
	m_Bpm = result.bpm;
	m_BeatsPerBar = result.beatsPerBar;
	m_AudioFile = result.audioFile;

	// ── 音声ファイルの読み込み ──
	if (!m_AudioFile.empty())
	{
		std::string fullPath = "assets/bgm/" + m_AudioFile; //曲を読み込んだ際は絶対にエディターで使ってる曲を入れること
		HRESULT hr = sound.LoadDynamic(fullPath, false);
		if (FAILED(hr))
			OutputDebugStringA(("[ChartObject] Failed to load audio: " + fullPath + "\n").c_str());
	}

	// ── メトロノームのカウントイン準備（1小節分）──
	float secondsPerBeatForCue = (m_Bpm > 0.0f) ? 60.0f / m_Bpm : 0.5f;
	m_CountInTime = secondsPerBeatForCue * m_BeatsPerBar;
	m_CountInNextBeat = 0;

	// ── 再生開始状態にリセット ──
	m_Started = false;
	m_AudioStarted = false;
	m_MusicTime = 0.0f;

	m_JudgeSystem.ResetJudgeCounts();

	m_ScoreDisplay.SetValue(0, true);

	m_Special.Reset(m_TotalNote);
	ChargeAura::Instance().Cancel();
	m_SpecialBeam.Cancel();
	m_WeakPoint.Reset();
	m_AttackOrbs.Clear();
	m_ImpactFx.Clear();
	m_LongShield.Reset();
	m_Hype.Reset();
	m_LastLowPass = -100.0f;
	m_LastPressedLane = 2;
	m_NoisePlaying = false;
	m_FinishChecked = false;

	for (auto& t : m_LastOrbTime) t = -100.0f;

	OutputDebugStringA(("[ChartPlayObject] Loaded: charts/" + name + ".json notes=" +
		std::to_string(m_Notes.size()) + " beams=" + std::to_string(m_BeamSystem.GetBeamCount()) + "\n").c_str());
	return true;
}
bool ChartObject::IsEnemyDefeated() const
{
	return m_EnemyRef && m_EnemyRef->IsDefeated();
}
void ChartObject::DrawLaneGuideLines(Camera* cam)
{
	if (!m_BeamSystem.IsModelLoaded()) return; // 線描画にはビームモデルの仕組みを流用しているため必須

	for (auto& note : m_Notes)
	{
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		float et = note.IsLong() ? note.arrivalTime : note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		Vector3 color = GetLaneColor(note.lane);

		// ノーツ本体と同じ出現点・判定点を線の両端にする
		Vector3 from = note.spawnPos;
		Vector3 to = note.judgePos;

		m_BeamSystem.DrawBeamSegmentWithColor(cam, from, to, 3.0f, 3.0f, color, 0.1f);
	}
}

void ChartObject::AddPauseOffset(std::chrono::steady_clock::duration offset)
{
	m_StartClock += offset;
	m_LastUpdateClock += offset;
}

void ChartObject::StopLongSounds()
{
	sound.Stop(SOUND_CHARGE);
}

//移動するロング　tau秒に判定ラインへ届く部分が今どこにあるか
DirectX::SimpleMath::Vector3 ChartObject::LongPointAt(const ChartNote& n, float tau) const
{
	constexpr float kTurnBlend = 0.06f;	//曲がりにかける秒数
	float x = m_JudgeCircles[n.lane].GetPosition(m_MusicTime).x;
	int ln = n.lane;
	for (const auto& tr : n.turns)
	{
		if (tau < tr.time) break;
		float from = m_JudgeCircles[ln].GetPosition(m_MusicTime).x;
		float to = m_JudgeCircles[tr.lane].GetPosition(m_MusicTime).x;
		float k = std::clamp((tau - tr.time) / kTurnBlend, 0.0f, 1.0f);
		k = k * k * (3.0f - 2.0f * k);
		x = from + (to - from) * k;
		ln = tr.lane;
	}
	Vector3 base = m_JudgeCircles[n.lane].GetPosition(m_MusicTime);
	float hs = Game::GetInstance()->GetHighSpeed();
	float z = (tau - m_MusicTime) / m_HighwayApproach * m_HighwayDepth * hs;   // 届くのが先ほど奥
	return DirectX::SimpleMath::Vector3(x, base.y, base.z + z);
}

void ChartObject::DrawHighwayLongNotes(Camera* cam)
{
	constexpr float kStep = 0.01f;	//何秒ごとに点を打つか　小さいほど滑らか
	const Vector3 kGlowColor(0.6f, 0.35f, 1.0f);	//色を変えてるところ

	Renderer::SetBlendState(BS_ADDITIVE);
	for (auto& note : m_Notes)
	{
		if (!note.IsLong() || note.judged) continue;
		float from = std::max(m_MusicTime, note.arrivalTime);			//もう受けた部分は描かない
		float to = std::min(note.time, m_MusicTime + m_HighwayApproach);//出現位置より奥は見えない
		if (from >= to) continue;

		bool started = (m_MusicTime >= note.arrivalTime);
		bool held = false;
		if (started)
			for (int ln = 0; ln < 6; ln++)
				if (note.AcceptsLane(ln, m_MusicTime, JudgeSystem::kTurnGrace) && Input::GetKeyPress(kLaneKeys[ln])) { held = true; break; }
		bool missedLook = started && !held;   // 受けられていない間は暗く

		//手前から奥へ点を並べる
		m_LongPoints.clear();
		bool continuesBeyond = (to < note.time);	//奥の出現位置の先にもまだ続いてるか
		for (float t = from;; t += kStep)
		{
			float tt = std::min(t, to);
			LongBeamRibbon::Point p;
			p.pos = LongPointAt(note, tt) + Vector3(0.0f, 4.0f, 0.0f);	//床から少し浮かせる
			Vector3 c = missedLook ? Vector3(0.4f, 0.4f, 0.4f) : GetLaneColor(note.LaneAt(tt));
			float fade = 1.0f;
			if (continuesBeyond)	//奥から現れるように奥の端を薄くする
				fade = std::clamp((to - tt) / (m_HighwayApproach * 0.15f), 0.0f, 1.0f);
			p.color = Color(c.x, c.y, c.z, fade);
			m_LongPoints.push_back(p);
			if (tt >= to)break;
		}
		m_LongBeam.Draw(cam, m_LongPoints,
			missedLook ? 60.0f : 90.0f,      // 帯の幅
			missedLook ? 0.35f : 1.0f,       // 明るさ
			m_MusicTime, kGlowColor);
	}
	Renderer::SetBlendState(BS_NONE);
}

void ChartObject::DrawSpawnMagicCircle(Camera* cam, const ChartNote& note)
{
	float st = note.GetSpawnTime();
	float et = note.IsLong() ? note.arrivalTime : note.time;	//判定・到達時刻で描画終了
	if (m_MusicTime < st || m_MusicTime > et) return;

	float duration = et - st;
	float progress = (duration > 0.0001f) ? (m_MusicTime - st) / duration : 1.0f;
	progress = std::clamp(progress, 0.0f, 1.0f);

	Vector3 pos = note.spawnPos;	//出現地点に固定

	Vector3 glowColor(0.8f, 0.2, 1.0f);
	Vector3 tintColor(0.05f, 0.05f, 0.05f);

	m_MagicCircleQuad.Draw(pos, 200.0f, progress, glowColor, tintColor);
}

bool ChartObject::IsHoldingLong(const ChartNote& n) const
{
	if (m_MusicTime < n.arrivalTime || m_MusicTime >= n.time) return false;
	if (m_AutoMode) return true;
	for (int ln = 0; ln < 6; ln++)
	{
		if (n.AcceptsLane(ln, m_MusicTime, JudgeSystem::kTurnGrace) && Input::GetKeyPress(kLaneKeys[ln]))
		{
			return true;
		}
	}
	return false;
}

//押してる間のループ音と曲がった時の音
//押している間の音（拍に合わせて一定間隔）と、曲がった時の音
void ChartObject::UpdateLongSounds()
{
	constexpr int kHoldSePerBeat = 2;	// 1拍に何回鳴らすか（2なら8分音符ごと）
	float beat = (m_Bpm > 0.0f) ? 60.0f / m_Bpm : 0.5f;
	float interval = beat / kHoldSePerBeat;

	for (auto& n : m_Notes)
	{
		if (!n.IsLong() || n.judged) continue;
		bool holding = IsHoldingLong(n);

		//曲がり点の時刻を過ぎたら一回だけ鳴らす
		while (n.nextTurnIndex < (int)n.turns.size() && m_MusicTime >= n.turns[n.nextTurnIndex].time)
		{
			if (holding) sound.Play(SOUND_CLAMPU);
			n.nextTurnIndex++;
		}

		//押している間の音：先端が届いた時刻から interval ごと
		//（先端が届いた瞬間はタップの判定音が鳴るので、1つ目は1間隔あとから）
		if (n.nextHoldSeTime < 0.0f) n.nextHoldSeTime = n.arrivalTime + interval;
		bool due = false;
		while (m_MusicTime >= n.nextHoldSeTime && n.nextHoldSeTime < n.time)
		{
			due = true;
			n.nextHoldSeTime += interval;
		}
		if (due && holding) /*sound.Play(SOUND_FINGERSNAP)*/;
	}
}

// side > 0 : S/D/F側(index 0,1,2)、side <= 0 : J/K/L側(index 3,4,5)
DirectX::SimpleMath::Vector3 ChartObject::GetPlayerSideAnchor(int side) const
{
	int startLane = (side > 0) ? 0 : 3;
	Vector3 sum = Vector3::Zero;
	for (int i = 0; i < 3; i++)
	{
		sum += m_JudgeCircles[startLane + i].GetPosition(m_MusicTime);
	}
	return sum / 3.0f;
}


// ============================================================
// Update : 毎フレームのゲームロジック更新。
//   時間経過→判定円の追従→カウントイン→ミス判定→接近音→
//   各種判定処理→判定済みノーツの削除→ビーム更新、の順で行う。
// ============================================================
void ChartObject::Update()
{
	const auto nowClock = std::chrono::steady_clock::now();

	// 初回フレームで開始時刻を記録
	if (!m_Started)
	{
		m_StartClock = nowClock;
		m_LastUpdateClock = nowClock;
		m_Started = true;
	}
	const float elapsed =
		std::chrono::duration<float>(nowClock - m_StartClock).count();

	// カウントイン中は-m_CountInTime〜0秒になり、0秒が譜面再生開始時刻
	m_MusicTime = elapsed - m_CountInTime;

	//m_MusicTime >= 0 に音楽再生を開始
	if (!m_AudioStarted && m_MusicTime >= 0.0f)
	{
		m_AudioStarted = true;
		if (!m_AudioFile.empty())
		{
			sound.PlayDynamic();
		}
	}

	const float deltaTime =
		std::chrono::duration<float>(nowClock - m_LastUpdateClock).count();
	m_LastUpdateClock = nowClock;
	m_ScreenEffects.Update(deltaTime);

	//判定バッドの光具合
	{
		constexpr float kLaneGlowFall = 5.0f;	//1秒あたりに暗くなる量
		for (int i = 0; i < 6; i++)
		{
			if (Input::GetKeyPress(kLaneKeys[i]))
			{
				m_LaneGlow[i] = 1.0f;
			}
			else
			{
				m_LaneGlow[i] = std::max(0.0f, m_LaneGlow[i] - kLaneGlowFall * deltaTime);
			}
		}
	}

	// 判定円の動きに合わせて、未判定ノーツのjudgePosを追従させる
	// （フリックはjudgePosがenemy側固定なので対象外）
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (note.IsFlick()) continue;
		if (note.lane < 0 || note.lane >= (int)m_JudgeCircles.size()) continue;
		note.judgePos = m_JudgeCircles[note.lane].GetPosition(m_MusicTime);
	}

	// パーティクルの更新（寿命切れは削除）
	for (auto it = m_Particle.begin(); it != m_Particle.end();)
	{
		it->Update(deltaTime);
		if (it->IsStop())
			it = m_Particle.erase(it);
		else
			++it;
	}


	// ── ロングノーツを保持成功中のレーンに、キラキラした粉パーティクルを継続的に出す ──
	{
		constexpr float kSparkleInterval = 0.04f;	// 何秒おきに1粒出すか。小さいほど密になる（重くなる）
		bool holding[6] = { false,false,false,false,false,false };
		for (auto& note : m_Notes)
		{
			if (!note.IsLong()) continue;
			if (note.judged) continue;
			if (note.lane < 0 || note.lane >= 6) continue;
			if (m_MusicTime < note.arrivalTime || m_MusicTime > note.time) continue;	//判定圏外は無視
			int ln = note.LaneAt(m_MusicTime);	//今線が来ているレーン
			if (!Input::GetKeyPress(kLaneKeys[ln])) continue;
			holding[ln] = true;
		}
		for (int lane = 0; lane < 6; lane++)
		{
			if (!holding[lane])
			{
				m_BarrierSparkleTimer[lane] = 0.0f;
				continue;
			}
			m_BarrierSparkleTimer[lane] += deltaTime;
			while (m_BarrierSparkleTimer[lane] >= kSparkleInterval)
			{
				m_BarrierSparkleTimer[lane] -= kSparkleInterval;

				Vector3 pos = m_JudgeCircles[lane].GetPosition(m_MusicTime);
				Vector3 laneColor = GetLaneColor(lane);
				float deg = (float)(rand() % 360);

				Particle p(
					pos,
					0.5f,                                  // 寿命（短め）
					25.0f,                                 // 重力（軽くふわっと落ちる）
					Color(laneColor.x, laneColor.y, laneColor.z, 1.0f),
					Color(laneColor.x, laneColor.y, laneColor.z, 0.0f),
					0.4f,                                  // フェード開始位置
					16.0f,                                 // 開始サイズ（小さい粉）
					2.0f,                                  // 終了サイズ
					Vector3(35.0f, 60.0f, 35.0f),          // 初速
					deg
				);
				m_Particle.push_back(p);
			}
		}
	}
	//ロングノーツ成功中のレーンパーティクル
	{
		if (!kHighwayMode)
		{
			// ふよふよ浮くアンビエントパーティクル（ロングノーツの周り、常時）
			constexpr float kGraceTime = 0.5f;
			constexpr float kAmbientInterval = 0.12f;

			for (int lane = 0; lane < 6; lane++)
			{
				bool spawnedThisLane = false;
				for (auto& note : m_Notes)
				{
					if (!note.IsLong()) continue;
					if (note.lane != lane) continue;
					if (note.judged) continue;

					float displayStart = note.GetSpawnTime();
					float extendedEnd = note.time + kGraceTime;
					if (m_MusicTime < displayStart || m_MusicTime > extendedEnd) continue;

					spawnedThisLane = true;

					m_LongAmbientParticleTimer[lane] += deltaTime;
					if (m_LongAmbientParticleTimer[lane] >= kAmbientInterval)
					{
						m_LongAmbientParticleTimer[lane] = 0.0f;

						// ノーツの経路上のランダムな地点を選ぶ
						float sampleT = (float)rand() / RAND_MAX;
						float sampleTime = displayStart + (m_MusicTime - displayStart) * sampleT;

						Vector3 spawnPos = NotePath::EvaluateLongNotePosition(note, sampleTime);
						Vector3 laneColor = GetLaneColor(lane);
						float deg = (float)(rand() % 360);

						Particle p(
							spawnPos,
							1.2f,                                              // lifeTime
							8.0f,                                              // gravity
							Color(laneColor.x, laneColor.y, laneColor.z, 0.8f),// 開始色
							Color(laneColor.x, laneColor.y, laneColor.z, 0.0f),// 終了色（透明にフェード）
							0.5f,                                              // フェード開始位置
							18.0f, 6.0f,                                       // 開始/終了サイズ
							Vector3(20.0f, 15.0f, 20.0f),                      // 初速
							deg
						);
						m_LongAmbientParticles.push_back(p);
					}
					break; // このレーンは1本のロングだけ処理すれば十分
				}
				if (!spawnedThisLane) m_LongAmbientParticleTimer[lane] = 0.0f;
			}

			// 経過したアンビエントパーティクルを削除・更新
			for (auto& p : m_LongAmbientParticles) { p.Update(deltaTime); }
			m_LongAmbientParticles.erase(
				std::remove_if(m_LongAmbientParticles.begin(), m_LongAmbientParticles.end(),
					[](Particle& p) { return p.IsStop(); }),
				m_LongAmbientParticles.end()
			);
		}

	}
	m_ComboHud.Update(deltaTime);

	//音の盛り上げ
	m_Hype.Update(deltaTime, m_JudgeSystem.GetComboCount());
	const float h = m_Hype.Get01();
	{
		//盛り上がるほど曲の高い音が聞こえる
		float hz = 0.0f;	//0=フィルターなし
		if (h < 0.999f) hz = kLowPassMinHz * std::pow(kLowPassOpenHz / kLowPassMinHz, h);
		if (std::fabs(hz - m_LastLowPass) > 1.0f)
		{
			sound.SetLowPassDynamic(hz);
			m_LastLowPass = hz;
		}
	}

	if (m_NoiseMode)
	{
		//ノイズ
		{
			bool musicOn = m_NoiseMode && m_AudioStarted && !m_AudioFile.empty() && !IsAudioFinish();
			if (musicOn && !m_NoisePlaying)
			{
				sound.Play(SOUND_NOISE);
				m_NoisePlaying = true;
			}
			else if (!musicOn && m_NoisePlaying)
			{
				sound.Stop(SOUND_NOISE);
				m_NoisePlaying = false;
			}
			float quiet = 1.0f - h;	//盛り上がってない度合い1->0
			sound.SetVolume(SOUND_NOISE, kNoiseVolume * quiet * quiet);
		}
	}


	m_ScoreDisplay.SetValue(ScoreRule::Calc(
		m_JudgeSystem.GetPerfectCount(), m_JudgeSystem.GetGreatCount(), m_JudgeSystem.GetGoodCount(), m_TotalNote
	));
	m_ScoreDisplay.Update();

	if (m_EnemyRef) m_EnemyHp.SetRatio(m_EnemyRef->GetHpRatio());
	m_EnemyHp.Update(deltaTime);

	if(!m_FullCombo && IsAudioFinish() && IsFullCombo())
	{
		m_ComboHud.TriggerFullCombo();
		m_FullCombo = true;
		//sound.Play(SOUND_FULLCOMBO);
	}

	// メトロノームのカウントイン音（1小節分、拍ごとにクリック音を鳴らす）
	if (m_Bpm > 0.0f && m_BeatsPerBar > 0)
	{
		float secondsPerBeat = 60.0f / m_Bpm;
		while (m_CountInNextBeat < m_BeatsPerBar)
		{
			float beatTime = -m_CountInTime + m_CountInNextBeat * secondsPerBeat;
			if (m_MusicTime < beatTime) break; // まだこの拍のタイミングに来ていない

			bool isLastBeat = (m_CountInNextBeat == m_BeatsPerBar - 1);
			sound.Play(isLastBeat ? SOUND_LABEL_METRO_KATI : SOUND_LABEL_METRO_TI);
			m_CountInNextBeat++;
		}
	}

	// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	//m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);

	// 接近音（判定時刻の1小節前になったら一度だけ再生）
	float secondPerBar = (m_BeatsPerBar > 0 && m_Bpm > 0.0f) ? (60.0f / m_Bpm) * m_BeatsPerBar : 2.0f;
	for (auto& note : m_Notes)
	{
		if (note.judged || note.approcachPlayed) continue;

		float cueTime = note.time - secondPerBar;
		if (m_MusicTime >= cueTime)
		{
			//sound.Play(SOUND_SEKKIN);	//いる？
			note.approcachPlayed = true;
		}
	}

	constexpr float kTapAuraInterval = 0.08f;
	for (auto& note : m_Notes)
	{
		if (!note.IsTap()) continue;
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		if (m_MusicTime < st || m_MusicTime > note.time) continue;

		m_TapAuraTimer[note.lane] += deltaTime;
		if (m_TapAuraTimer[note.lane] >= kTapAuraInterval)
		{
			m_TapAuraTimer[note.lane] = 0.0f;

			Vector3 pos = NotePath::EvaluateNotePosition(note, m_MusicTime);
			Vector3 laneColor = GetLaneColor(note.lane);
			float deg = (float)(rand() % 360);
			float radius = 20.0f + (float)(rand() % 20); // 球の周囲にランダムなオフセット
			Vector3 spawnPos = pos + Vector3(cosf(deg) * radius, ((float)(rand() % 100) / 100.0f - 0.5f) * 20.0f, sinf(deg) * radius);

			Particle p(
				spawnPos,
				0.5f,                                              // lifeTime
				5.0f,                                              // gravity(軽くふわっと)
				Color(laneColor.x, laneColor.y, laneColor.z, 0.7f),
				Color(laneColor.x, laneColor.y, laneColor.z, 0.0f),
				0.3f,
				3.0f, 1.0f,											//スケール調整　左から始まって消えるときの大きさ右
				Vector3(10.0f, 8.0f, 10.0f),
				deg
			);
			m_TapAuraParticles.push_back(p);
		}
	}
	for (auto& p : m_TapAuraParticles) p.Update(deltaTime);
	m_TapAuraParticles.erase(
		std::remove_if(m_TapAuraParticles.begin(), m_TapAuraParticles.end(), [](Particle& p) { return p.IsStop(); }),
		m_TapAuraParticles.end());

	if (m_AutoMode)
	{
		m_JudgeSystem.UpdateAutoJudgge(m_Notes, m_MusicTime, sound);
	}
	else
	{
		//TAPノーツの自動ミス判定
		m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);
		m_JudgeSystem.MarkFlickAutoMiss(m_Notes, m_MusicTime);

		int playerSideForFlick = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		for (int lane = 0; lane < 6; lane++)
		{
			m_JudgeSystem.JudgeLane(kLaneKeys[lane], lane, m_Notes, m_MusicTime, sound);
		}
		{
			NoteJudgeContext ctx;
			ctx.musicTime = m_MusicTime;
			ctx.autoMode = m_AutoMode;
			ctx.judge = &m_JudgeSystem;
			ctx.sound = &sound;
			ctx.isLaneHeld = [](int lane)->bool {return Input::GetKeyPress(kLaneKeys[lane]); };
			for (auto& n : m_Notes)
			{
				if (n.judged) continue;
				if (NoteBehavior* b = NoteRegistry::Find(n.type))b->Judge(n, ctx);
			}
		}

		// フリック・ロングノーツの判定
		m_JudgeSystem.JudgeFlickNotes(m_Notes, m_MusicTime, playerSideForFlick, sound);
		m_JudgeSystem.JudgeLongNoteHold(m_Notes, m_MusicTime);
	}

	//ロングノーツの音を鳴らしたりするところ
	UpdateLongSounds();

	//ロングノーツの盾に始まり、曲がり、終点を知らせる
	UpdateLongShield();

	//必殺技発動
	//最後に押したレーンを覚える（チャージ中でロック前なら、狙いもそのレーンへ動く）
	for (int lane = 0; lane < 6; lane++)
	{
		if (Input::GetKeyTrigger(kLaneKeys[lane]))
		{
			m_LastPressedLane = lane;
			m_WeakPoint.SetAim(lane);
		}
	}

	//Space：溜め中なら発動、チャージ中ならロックオン
	if (Input::GetKeyTrigger(VK_SPACE))
	{
		if (m_Special.GetState() == SpecialMove::State::Gauge)
		{
			if (m_Special.TryActivate(m_MusicTime, m_Bpm, m_BeatsPerBar, m_Notes))
			{
				m_JudgeSystem.SetMissFressWindow(m_Special.GetChargeStart(), m_Special.GetChargeEnd());
				m_WeakPoint.BeginSpecial(m_MusicTime, m_Special.GetChargeEnd(), m_LastPressedLane);
			}
		}
		else if (m_WeakPoint.Lock())
		{
			sound.Play(SOUND_BEAM_DECITION);	//ロックオンの音
		}
	}
	m_Special.Update(deltaTime, m_MusicTime);
	m_SpecialBeam.Update(deltaTime);
	// 判定円の位置を渡す（弱点のレーンの判定円に印を出すため）
	for (int i = 0; i < 6 && i < (int)m_JudgeCircles.size(); i++)
		m_WeakPoint.SetLaneAnchor(i, m_JudgeCircles[i].GetPosition(m_MusicTime));
	if (m_EnemyRef) m_WeakPoint.Update(deltaTime, m_MusicTime, m_Bpm, m_BeatsPerBar, m_EnemyRef->GetPosition());
	if (m_EnemyRef) m_LongShield.Update(deltaTime, m_EnemyRef->GetPosition());
	m_AttackOrbs.Update(deltaTime);
	m_ImpactFx.Update(deltaTime);
	m_SpGauge.Update(deltaTime, m_Special);
	ChargeAura::Instance().Update(deltaTime, m_Special.GetChargeProgress(m_MusicTime));

	// 判定済み(judged=true)になったノーツをまとめて削除
	// （TAPの判定はJudgeLaneで即座にjudged=trueにするが、実削除はここでまとめて行う）
	m_Notes.erase(
		std::remove_if(m_Notes.begin(), m_Notes.end(),
			[](const ChartNote& n) { return n.judged; }),
		m_Notes.end());

	//全部のノーツの判定が終わったら最後のノーツをとどめノーツにする
	if (!m_FinishChecked && m_AudioStarted && m_TotalNote > 0 && m_Notes.empty())
	{
		m_FinishChecked = true;
		if (m_EnemyRef && m_EnemyRef->IsDying()) m_EnemyRef->Finish();
		else;	//瀕死まで行けなかった時の演出追加用
	}

	UpdateLightEffects(deltaTime);
	// ホーミングビームの状態遷移とプレイヤー移動の更新
	{
		int playerSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		m_BeamSystem.Update(deltaTime, m_MusicTime, playerSide, GetPlayerSideAnchor(playerSide), sound, m_ScreenEffects);
	}

	//Camera::Layer::Worldのほうの目線をロングを押し切った瞬間に上げて一拍後戻す
	{
		float beat = (m_Bpm > 0.0f) ? 60.0f / m_Bpm : 0.5f;
		float a = (m_MusicTime - m_GazeKickTime) / beat;	//経過を拍で数える
		float k = 0.0f;
		if (a >= 0.0f && a <= 1.0f)
		{
			float up = std::min(a / 0.15f, 1.0f);	//0.15拍で上がり切る
			float down = 1.0f - std::max((a - 0.4f) / 0.6f, 0.0f);	//0.4拍から戻る
			k = up * down;
			k = k * k * (3.0f - 2.0f * k);	//smoothstep 動き始めと終わりを滑らかに
		}
		Game::GetInstance()->GetCamera()->SetWorldOffset(
			0.0f, DirectX::XMConvertToRadians(3.0f) * k, 0.0f
		);
	}
}
// ============================================================
// OnJudge : 判定がヒットした瞬間に呼ばれるフック。
//   コンボを加算し、判定円の位置に花びらパーティクルを100個生成する。
// ============================================================
void ChartObject::OnJudge(int laneIndex, int rank)
{
	m_Special.AddHit(rank, false);
	m_ComboHud.TriggerBarrier(laneIndex);
	m_ComboHud.TriggerRankPop(laneIndex, rank);
	m_ComboHud.TriggerComboPop();

	//叩くたびに少しダメージを与える　ロングの盾が張られている間はダメージなし
	if (m_EnemyRef && m_TotalNote > 0 && !m_LongShield.IsUp()) m_EnemyRef->Damage(60.0f / m_TotalNote);

	if (laneIndex < 0 || laneIndex >= 6) return;
	m_LaneGlow[laneIndex] = 1.0f;	//判定したレーンのバッドを光らせる
	Vector3 pos = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	//判定円から、敵の体のランダムな位置へ光を飛ばす。PERFECT は太い金の光（同じレーンは kOrbMinInterval に1つまで）
	bool perfect = (rank == 0);
	if (m_EnemyRef && m_MusicTime - m_LastOrbTime[laneIndex] >= kOrbMinInterval)
	{
		Vector3 target = m_LongShield.IsUp() ? m_LongShield.GetRandomSurfacePos() : m_WeakPoint.GetRandomBodyPos();
		m_AttackOrbs.Launch(laneIndex, pos, target, GetLaneColor(laneIndex), perfect);
	}

	for (int i = 0; i < 100; i++)
	{
		float deg = (float)(rand() % 360);
		Particle p(
			pos,
			0.5f,                          // 寿命
			50.0f,                         // 重力
			Color(1, 1, 1, 1),				// 開始色
			Color(1, 1, 1, 0),				// 終了色（透明にフェード）
			0.7f,                          // フェード開始位置（終盤30%だけフェード）
			60.0f,                         // 開始サイズ
			20.0f,                         // 終了サイズ
			Vector3(80, 120, 80),          // 初速
			deg
		);
		m_Particle.push_back(p);
	}
	ChartKeyLightEffect le;
	le.lane = laneIndex;
	le.startTime = m_MusicTime;
	le.duration = 0.4f;
	le.intensity = 2.5f;
	le.origin = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	m_LightEffects.push_back(le);
}

// ============================================================
// OnEmptyLanePress : そのレーンに判定対象のTAPノーツが無いときに
//   空押ししてしまった場合のフック（元々JudgeLane内にあった処理）。
// ============================================================
void ChartObject::OnEmptyLanePress(int laneIndex)
{
	ChartKeyLightEffect le;
	le.lane = laneIndex;
	le.startTime = m_MusicTime;
	le.duration = 0.6f;
	le.intensity = 2.2f;
	le.origin = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	m_LightEffects.push_back(le);
}

void ChartObject::UpdateLightEffects(float deltaTime)
{
	m_LightEffects.erase(
		std::remove_if(m_LightEffects.begin(), m_LightEffects.end(),
			[this](const ChartKeyLightEffect& le) {
				return (m_MusicTime - le.startTime) >= le.duration;
			}),
		m_LightEffects.end());
}

void ChartObject::DrawLightEffects(Camera* cam)
{
	if (!m_BeamSystem.IsModelLoaded()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ADDITIVE);

	constexpr float kMaxDistance = 500.0f;	//柱の全体的な長さ
	constexpr float kBaseWidth = 360.0f;		//柱の根元の太さ
	constexpr float kTopWidth = 550.0f;		//柱の先端の太さ
	constexpr float kLampSize = 140.0f;		//根元の丸い光の大きさ

	//柱は画面の上方向へまっすぐ伸ばす
	Matrix invView = cam->GetViewMatrix().Invert();
	Vector3 screenUpInWorld(invView._21, invView._22, invView._23);
	screenUpInWorld.Normalize();

	for (auto& le : m_LightEffects)
	{
		if (le.lane < 0 || le.lane >= (int)m_JudgeCircles.size()) continue;

		float age = m_MusicTime - le.startTime;
		float t = std::clamp(age / le.duration, 0.0f, 1.0f);

		//アニメーションカーブ
		//立ち上がりは一瞬でマックスに到達、そこから減衰
		float growT = std::clamp(t / 0.15f, 0.0f, 1.0f);
		float riseEase = 1.0f - (1.0f - growT) * (1.0f - growT);
		float decay = 1.0f - t;		//線形フェード
		decay = decay * decay;		//後半を急速に消す

		float envelope = riseEase * decay;	//全体の強さ
		if (envelope <= 0.001f) continue;

		//根元はその時の判定円の位置
		Vector3 from = m_JudgeCircles[le.lane].GetPosition(m_MusicTime);
		Vector3 color = GetLaneColor(le.lane);
		float reach = kMaxDistance * (0.3f + 0.7f * riseEase);	//出た瞬間に伸びる

		m_HitPillar.Draw(cam, from, screenUpInWorld,
			reach, kBaseWidth, kTopWidth,
			color, le.intensity * envelope, kLampSize * envelope);
	}
	Renderer::SetBlendState(BS_NONE);
}

void ChartObject::UpdateLongShield()
{
	for (auto& n : m_Notes)
	{
		if (!n.IsLong() || n.shieldDone) continue;

		//先端が判定ラインに届いたら展開
		if (!n.shieldStarted)
		{
			if (m_MusicTime < n.arrivalTime) continue;
			n.shieldStarted = true;
			m_LongShield.BeginLong((int)n.turns.size() + 1);
		}
		//このフレームで終わったか
		bool ending = n.judged || m_MusicTime >= n.time;

		//曲がり角：曲がってからkTurnGrace秒たったら新しいレーンを押してるか調べる
		while (n.shieldTurnIndex < (int)n.turns.size())
		{
			const LaneTurn& tr = n.turns[n.shieldTurnIndex];
			bool onNewLane = m_AutoMode || Input::GetKeyPress(kLaneKeys[tr.lane]);	//新しいレーンを押しているか

			if (!ending)
			{
				if (m_MusicTime < tr.time)break;	//まだ曲がり角に来ていない
				if (!onNewLane && m_MusicTime < tr.time + JudgeSystem::kTurnGrace)break;	//まだ押し変えていないが猶予がある
			}
			bool ok = ending ? n.shieldLastHeld : onNewLane;
			m_LongShield.Checkpoint(ok);
			n.shieldTurnIndex++;
		}
		//終点：最後のフレームで押せていたか
		if (ending)
		{
			m_LongShield.Checkpoint(n.shieldLastHeld);
			m_LongShield.EndLong();
			n.shieldDone = true;
			continue;
		}
		n.shieldLastHeld = IsHoldingLong(n);	//次のフレームのために今押せてるかを覚えておく
	}
}

void ChartObject::SetupEnemyEvents()
{
	if (!m_EnemyRef) return;

	//瀕死になった瞬間
	m_EnemyRef->SetOnDying([this]()
		{
			m_ScreenEffects.TriggerScreenShake(0.2f, 6.0f);
			sound.Play(SOUND_MIRROR_BREAK_SMALL);
		});

	//とどめ
	m_EnemyRef->SetOnDefeated([this]()
		{
			m_ScreenEffects.TriggerFlash(0.1f);
			sound.Play(SOUND_MIRROR_BREAK_SMALL);
		});

	//爆発した瞬間
	m_EnemyRef->SetOnExploded([this]()
		{
			Vector3 c = m_EnemyRef->GetPosition();
			m_ImpactFx.SpawnCritical(c, Vector3(1.0f, 0.85f, 0.4f), 1.0f);
			m_ScreenEffects.TriggerScreenShake(0.5f, 16.0f);
			m_ScreenEffects.TriggerFlash(0.2f);
			sound.Play(SOUND_MIRROR_BREAK_SMALL);
		});
}


// ============================================================
// DrawLongNotes : ロングノーツを、判定キーへ撃ち込まれる「ビーム」として描画する。
//   以前はsentan(先端)/dou(本体・鎖)/o(終端の輪)の3パーツのFBXモデルを
//   並べて描画していたが、視認性向上のためHomingBeamSystemの
//   DrawBeamSegmentWithColorを流用したビーム表現に変更した。
//
//   時間の流れ：
//     displayStart(出現) → arrivalTime(先端が判定ラインに到達) → endTime(終端が到達し消える)
// ============================================================
//void ChartObject::DrawLongNotes(Camera* cam)
//{
//	constexpr float kGraceTime = 0.5f;			// 判定ラインを越えても先端が進み続ける猶予秒数
//	constexpr float kCoreThicknessX = 16.0f;	// ビームの芯（明るい部分）の太さ。見た目調整用
//	constexpr float kCoreThicknessY = 16.0f;
//	constexpr float kGlowThicknessX = 55.0f;	// ビームのグロー（柔らかく広がる部分）の太さ。見た目調整用
//	constexpr float kGlowThicknessY = 55.0f;
//
//	Renderer::SetBlendState(BS_ADDITIVE);
//
//	for (auto& note : m_Notes)
//	{
//		if (!note.IsLong()) continue;
//		if (note.judged) continue;
//
//		float displayStart = note.GetSpawnTime();	// 出現時刻
//		float arrivalTime = note.arrivalTime;		// 先端が判定ラインに着く時刻
//		float endTime = note.time;					// 終端が判定ラインに着く時刻（消滅予定）
//		float holdDuration = endTime - arrivalTime;	// ノーツの長さ（先端と終端の時間差）
//
//		// 出現〜到達が短すぎる（≒0秒）場合は0除算やワープ表示を避けるためスキップ
//		if (arrivalTime - displayStart <= 0.0001f) continue;
//		if (holdDuration <= 0.0001f) holdDuration = 0.0001f;
//
//		// 終端が消えるまで（猶予込み）の間だけ表示対象とする
//		float extendedEnd = endTime + kGraceTime;
//		if (m_MusicTime < displayStart || m_MusicTime > extendedEnd) continue;
//
//		// ── ビームの先端（head）── arrivalTimeを過ぎてもkGraceTime秒だけ進み続ける
//		// ただし「判定に成功して保持中」の間は、魔法障壁でせき止められて見えるように
//		// 先端をarrivalTime（＝判定キー/障壁の位置）で止める。保持に失敗した瞬間、
//		// 堰き止めが外れて判定ラインの先まで突き抜ける
//		bool pestArrival = m_MusicTime >= arrivalTime;
//		bool isHeldNow = pestArrival && note.lane >= 0 && note.lane < 6 && Input::GetKeyPress(kLaneKeys[note.lane]);
//
//		float headTime = std::min(m_MusicTime, endTime);
//		bool blockedByBarrier = isHeldNow && headTime > arrivalTime;
//		if (blockedByBarrier)
//		{
//			headTime = arrivalTime;
//		}
//		Vector3 headPos = NotePath::EvaluateLongNotePosition(note, headTime);
//
//		// ── ビームの後端（tail）── holdDuration秒だけheadより遅れて追従する
//		float tailTime = headTime - holdDuration;
//		bool tailVisible = (tailTime >= displayStart);
//		float bodyStart = tailVisible ? tailTime : displayStart;
//		Vector3 tailPos = NotePath::EvaluateLongNotePosition(note, bodyStart);
//
//		// 到達済みなのに今は押されていない間は、暗く・色を落として「今は取れていない感」を出す
//		bool showAsMissedLook = pestArrival && !isHeldNow;
//		Vector3 laneColor = GetLaneColor(note.lane);
//		Vector3 beamColor = showAsMissedLook ? Vector3(0.4f, 0.4f, 0.4f) : laneColor;
//		//Vector3 beamColor = laneColor;
//		float coreIntensity = showAsMissedLook ? 0.3f : 2.0f;
//		float glowIntensity = showAsMissedLook ? 0.15f : 0.7f;
//		float a = 0.3f;
//		float b = 0.15f;
//
//		// 芯（細く明るい）
//		m_BeamSystem.DrawBeamSegmentWithColor(cam, tailPos, headPos,
//			kCoreThicknessX, kCoreThicknessY, beamColor, coreIntensity);
//		// グロー（太く柔らかい）
//		m_BeamSystem.DrawBeamSegmentWithColor(cam, tailPos, headPos,
//			kGlowThicknessX, kGlowThicknessY, beamColor, glowIntensity);
//	}
//
//	Renderer::SetBlendState(BS_NONE);
//}

//3つ並べる系ロングノーツの方
void ChartObject::DrawLongNotes(Camera* cam)
{
	constexpr float kGraceTime = 0.5f;			// 判定ラインを越えても先端が進み続ける猶予秒数
	constexpr float kBodySpacing = 0.01f;		// dou（本体）を敷き詰める間隔（秒）。小さいほど密になるが重くなる
	constexpr int   kMaxBodySegments = 60;		// dou描画数の安全上限（負荷対策）
	constexpr float kBodyWindowSeconds = kBodySpacing * kMaxBodySegments;

	// 各パーツごとに「専用モデルがあればそれを、無ければTAPモデルを」使う
	auto& sentanRenderer = m_LongSentanLoaded ? m_LongSentanRenderer : m_MeshRenderer;
	auto& sentanShader = m_LongSentanLoaded ? m_LongSentanShader : m_Shader;
	auto& sentanSubsets = m_LongSentanLoaded ? m_LongSentanSubsets : m_Subsets;
	auto& sentanMaterials = m_LongSentanLoaded ? m_LongSentanMaterials : m_Materials;
	auto& sentanTextures = m_LongSentanLoaded ? m_LongSentanTextures : m_Textures;

	auto& douRenderer = m_LongDouLoaded ? m_LongDouRenderer : m_MeshRenderer;
	auto& douShader = m_LongDouLoaded ? m_LongDouShader : m_Shader;
	auto& douSubsets = m_LongDouLoaded ? m_LongDouSubsets : m_Subsets;
	auto& douMaterials = m_LongDouLoaded ? m_LongDouMaterials : m_Materials;
	auto& douTextures = m_LongDouLoaded ? m_LongDouTextures : m_Textures;

	auto& oRenderer = m_LongOLoaded ? m_LongORenderer : m_MeshRenderer;
	auto& oShader = m_LongOLoaded ? m_LongOShader : m_Shader;
	auto& oSubsets = m_LongOLoaded ? m_LongOSubsets : m_Subsets;
	auto& oMaterials = m_LongOLoaded ? m_LongOMaterials : m_Materials;
	auto& oTextures = m_LongOLoaded ? m_LongOTextures : m_Textures;

	for (auto& note : m_Notes)
	{
		if (!note.IsLong()) continue;
		if (note.judged) continue;

		float displayStart = note.GetSpawnTime();	// 出現時刻
		float arrivalTime = note.arrivalTime;		// 先端が判定ラインに着く時刻
		float endTime = note.time;					// 終端が判定ラインに着く時刻（消滅予定）
		float holdDuration = endTime - arrivalTime;	// ノーツの長さ（先端と終端の時間差）

		if (arrivalTime - displayStart <= 0.0001f) continue;
		if (holdDuration <= 0.0001f) holdDuration = 0.0001f;

		float extendedEnd = endTime + kGraceTime;
		if (m_MusicTime < displayStart || m_MusicTime > extendedEnd) continue;

		//進行方向に沿った回転行列
		//向きがおかしい場合はright/up/noteDirの並び順を入れ替えてね
		Vector3 noteDir = note.judgePos - note.spawnPos;
		Matrix noteBasis = Matrix::Identity;
		if (noteDir.LengthSquared() > 0.0001f)
		{
			noteDir.Normalize();
			Vector3 upHint = Vector3::Up;
			if (fabsf(noteDir.Dot(upHint)) > 0.98f) upHint = Vector3::Right;
			Vector3 right = upHint.Cross(noteDir);
			right.Normalize();
			Vector3 up = noteDir.Cross(right);
			up.Normalize();

			noteBasis = Matrix(
				right.x, right.y, right.z, 0.0f,
				up.x, up.y, up.z, 0.0f,
				noteDir.x, noteDir.y, noteDir.z, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		bool pestArrival = m_MusicTime >= arrivalTime;
		bool isHeldNow = pestArrival && note.lane >= 0 && note.lane < 6 && Input::GetKeyPress(kLaneKeys[note.lane]);

		float headTime = std::min(m_MusicTime, endTime);
		bool blockedByBarrier = isHeldNow && headTime > arrivalTime;
		if (blockedByBarrier)
		{
			headTime = arrivalTime;
		}
		Vector3 headPos = NotePath::EvaluateLongNotePosition(note, headTime);

		if (blockedByBarrier)
		{
			Vector3 travelDir = note.judgePos - note.spawnPos;
			if (travelDir.LengthSquared() > 0.0001f)
			{
				travelDir.Normalize();
				constexpr float kBarrierPushBack = 30.0f;
				headPos -= travelDir * kBarrierPushBack;
			}
		}

		float tailTime = headTime - holdDuration;
		bool tailVisible = (tailTime >= displayStart);
		float bodyStart = tailVisible ? tailTime : displayStart;
		float chainBodyStart = std::max(bodyStart, headTime - kBodyWindowSeconds);

		// ── 1) sentan（先端）を描画 ──
		{
			Matrix world = Matrix::CreateScale(40.0f) * Matrix::CreateRotationX(3.14f) * noteBasis * Matrix::CreateTranslation(headPos);
			ModelUtil::DrawModel(sentanRenderer, sentanShader, sentanSubsets, sentanMaterials, sentanTextures, world);
		}

		// ── 2) o（終端）を描画 ──
		if (tailVisible)
		{
			Vector3 tailPos = NotePath::EvaluateLongNotePosition(note, bodyStart);
			Matrix world = Matrix::CreateScale(20.0f) * noteBasis * Matrix::CreateTranslation(tailPos);
			ModelUtil::DrawModel(oRenderer, oShader, oSubsets, oMaterials, oTextures, world);
		}

		// ── 3) sentanでもoでもない部分は、douを一定間隔で並べて補間する ──
		{
			float bodyDuration = headTime - chainBodyStart;
			if (bodyDuration > 0.0001f)
			{
				int segments = std::max(2, (int)std::ceil(bodyDuration / kBodySpacing));
				segments = std::min(segments, kMaxBodySegments); // 描画数に上限をかけて負荷を抑える

				std::vector<Vector3> douPositions;
				douPositions.reserve(segments);

				for (int s = 1; s < segments; s++) // 両端(sentan/o)は上ですでに描画済みなので中間だけ
				{
					float p = (float)s / (float)segments;
					float sampleTime = bodyStart + bodyDuration * p;
					Vector3 pos = NotePath::EvaluateLongNotePosition(note, sampleTime);
					douPositions.push_back(pos);
					
					Matrix world = Matrix::CreateScale(30.0f) * noteBasis * Matrix::CreateTranslation(pos);
					ModelUtil::DrawModel(douRenderer, douShader, douSubsets, douMaterials, douTextures, world);
				}

				//魔力の残滓っぽいのを追加する
				Renderer::SetBlendState(BS_ADDITIVE);
				Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f);
				for (const Vector3& glowPos : douPositions)
				{
					Matrix glowWorld = Matrix::CreateScale(31.0f) * noteBasis * Matrix::CreateTranslation(glowPos);
					ModelUtil::DrawModelWithOverrideTexture(douRenderer, m_LongDouGlowShader, douSubsets, douMaterials, m_LongDouGlowTexture, glowWorld);
				}
				Renderer::SetBlendState(BS_NONE);
			}
		}
	}
}

// ============================================================
// DrawFlickNotes : 横フリックノーツの描画。
//   右フリック/左フリックで回転を変える（モデルの向きに応じて要調整）。
// ============================================================
void ChartObject::DrawFlickNotes(Camera* cam)
{
	auto& renderer = m_FlickModelLoaded ? m_FlickMeshRenderer : m_MeshRenderer;
	auto& shader = m_FlickModelLoaded ? m_FlickShader : m_Shader;
	auto& subsets = m_FlickModelLoaded ? m_FlickSubsets : m_Subsets;
	auto& materials = m_FlickModelLoaded ? m_FlickMaterials : m_Materials;
	auto& textures = m_FlickModelLoaded ? m_FlickTextures : m_Textures;

	for (auto& note : m_Notes)
	{
		if (!note.IsFlick()) continue;
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		float et = note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		Vector3 pos = NotePath::EvaluateNotePosition(note, m_MusicTime);

		// 右フリックと左フリックで回転を反転（※モデルの向きに応じて調整が必要）
		float rotY = note.IsFlickRight() ? 1.57f : -1.57f;

		Matrix world =
			Matrix::CreateScale(m_NoteScale * 5.7f) *
			Matrix::CreateRotationZ(1.57f) *
			Matrix::CreateRotationY(rotY) *
			Matrix::CreateTranslation(pos);

		ModelUtil::DrawModel(renderer, shader, subsets, materials, textures, world);
	}
}

// ============================================================
// Draw : 毎フレームの描画本体。
//   TAPノーツ本体 → ロングノーツ → フリックノーツ → 接近リング →
//   レーンアイコン/判定円ガイド → パーティクル → コンボ表示 → ビーム
//   の順に描画する。
// ============================================================
void ChartObject::Draw(Camera* cam)
{
	cam->SetShakeOffset(m_ScreenEffects.GetShakeOffset());
	// 3Dカメラを再アサート（他オブジェクトがTexture2Dで2Dに切り替えている場合があるため）
	cam->SetCamera(0);
	const Matrix view = cam->GetViewMatrix();

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		DrawSpawnMagicCircle(cam, note);
	}
	Renderer::SetBlendState(BS_NONE);

	Renderer::SetBlendState(BS_ADDITIVE);
	DrawLaneGuideLines(cam);
	Renderer::SetBlendState(BS_NONE);

	//判定ライン・判定バッド・レーンのトラック・拍の線
	//ノーツとかより先に描く
	if ((int)m_JudgeCircles.size() >= JudgeLineGuide::kLaneCount)
	{
		JudgeLineGuide::Frame gf = {};
		float highSpeed = Game::GetInstance()->GetHighSpeed();	//ノーツと同じくハイスピで奥行を伸ばす
		for (int i = 0; i < JudgeLineGuide::kLaneCount; i++)
		{
			gf.judgePos[i] = m_JudgeCircles[i].GetPosition(m_MusicTime);
			gf.trackOffset[i] = m_LaneTrackOffset[i] * highSpeed;
			gf.laneColor[i] = GetLaneColor(i);
			gf.laneGlow[i] = m_LaneGlow[i];
		}
		gf.musicTime = m_MusicTime;
		gf.bpm = m_Bpm;
		gf.beatsPerBar = m_BeatsPerBar;
		gf.approachSeconds = m_TrackApproachSeconds;
		m_JudgeGuide.Draw(cam, gf);
	}


	// ── ノーツ到達ガイド：左右から伸びて判定時刻ちょうどで円になるリング ──
	// 判定が一番近いノーツのリングが必ず一番手前に見えるよう、
	// 判定が遅い順に描いてから、判定が速いものを最後に描く。
	{
		std::vector<const ChartNote*> ringNotes;
		for (auto& note : m_Notes)
		{
			if (note.judged) continue;
			if (const NoteBehavior* b = NoteRegistry::Find(note.type))
				if (!b->Look(note).approachRing) continue;

			float st = note.GetSpawnTime();
			float et = note.IsLong() ? note.arrivalTime : note.time;
			if (m_MusicTime < st || m_MusicTime > et) continue;

			ringNotes.push_back(&note);
		}

		std::sort(ringNotes.begin(), ringNotes.end(),
			[](const ChartNote* a, const ChartNote* b)
			{
				float ta = a->IsLong() ? a->arrivalTime : a->time;
				float tb = b->IsLong() ? b->arrivalTime : b->time;
				return ta > tb; // 判定が遅い(時間が大きい)方を先に描く
			});

		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::SetDepthOverlayEnable(true); // 深度テストも無効化し、常に最前面に描画する
		for (const ChartNote* notePtr : ringNotes)
		{
			const ChartNote& note = *notePtr;
			float st = note.GetSpawnTime();
			float et = note.IsLong() ? note.arrivalTime : note.time;
			float duration = et - st;
			float t = (duration > 0.0001f) ? (m_MusicTime - st) / duration : 1.0f;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;

			Vector3 pos = note.judgePos;	// 終着点(判定位置)に固定

			//同じレーンで自分自分より判定が近いノーツが何個あるかを数える
			float myJudgeTime = note.IsLong() ? note.arrivalTime : note.time;
			int rank = 0;
			for (const ChartNote* other : ringNotes)
			{
				if (other == notePtr) continue;
				if (other->lane != note.lane) continue;
				float otherJudgeTime = other->IsLong() ? other->arrivalTime : other->time;
				if (otherJudgeTime < myJudgeTime)
					rank++;
			}
			float kBaseScale = 120.0f;
			if (note.IsFlick())
			{
				kBaseScale = 520.0f;
			}
			constexpr float kStateStep = 0.0f;	//重なったノーツ一つにつきこの数値だけ外に広がる
			float ringScale = kBaseScale + rank * kStateStep;

			Vector3 laneColor = GetLaneColor(note.lane);
			m_ClosingRingGuide.Draw(pos, ringScale, t, laneColor);
		}
		Renderer::SetDepthOverlayEnable(false);
		Renderer::SetBlendState(BS_NONE);
	}

	// ── TAPノーツ本体の描画 ──
	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();

	// ── ノーツ本体の描画 ──
	// ノーツ 1 つの、位置・色・大きさを出す（下の 2 つのループで使う）
	auto noteVisual = [&](const ChartNote& note, const NoteLook& look, NoteDrawContext& out) -> bool
		{
			if (note.judged) return false;
			float st = note.GetSpawnTime();
			float et = note.time;
			if (m_MusicTime < st || m_MusicTime > et) return false;	// 表示範囲外

			Vector3 c = note.chargeShot ? Vector3(1.0f, 0.8f, 0.25f) : GetLaneColor(note.lane);
			out.pos = NotePath::EvaluateNotePosition(note, m_MusicTime);	// Long ではない方
			out.color = c + (Vector3(1.0f, 1.0f, 1.0f) - c) * look.whiteMix;	// 白を混ぜる
			out.scale = m_NoteScale * look.scale;
			out.musicTime = m_MusicTime;	// 足す
			return true;
		};

	// 自分の形を持つ種類（トレースなど）を先に描く
	std::vector<const ChartNote*> drawnSelf;
	for (auto& note : m_Notes)
	{
		NoteBehavior* behavior = NoteRegistry::Find(note.type);
		if (!behavior) continue;
		NoteDrawContext dc;
		if (!noteVisual(note, behavior->Look(note), dc)) continue;
		if (behavior->Draw(note, dc)) drawnSelf.push_back(&note);
	}

	//タップと、自分の形を持たない種類を、タップのモデルで描く
	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();
	for (auto& note : m_Notes)
	{
		const NoteBehavior* behavior = NoteRegistry::Find(note.type);
		if (!note.IsTap() && !behavior) continue;	// タップと登録されたノーツだけ
		if (std::find(drawnSelf.begin(), drawnSelf.end(), &note) != drawnSelf.end()) continue;

		NoteLook look = behavior ? behavior->Look(note) : NoteLook();
		NoteDrawContext dx;
		if (!noteVisual(note, look, dx)) continue;

		// 本体
		{
			Matrix world = Matrix::CreateScale(dx.scale) * Matrix::CreateTranslation(dx.pos);
			Renderer::SetWorldMatrix(&world);
			for (size_t i = 0; i < m_Subsets.size(); i++)
			{
				m_Materials[m_Subsets[i].MaterialIdx]->SetDiffuse(
					DirectX::XMFLOAT4(dx.color.x, dx.color.y, dx.color.z, 1.0f));
				m_Materials[m_Subsets[i].MaterialIdx]->SetEmission(
					DirectX::XMFLOAT4(dx.color.x, dx.color.y, dx.color.z, 1.0f));
				m_Materials[m_Subsets[i].MaterialIdx]->Update();

				m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();
				if (m_Materials[m_Subsets[i].MaterialIdx]->isTextureEnable())
					m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();

				m_MeshRenderer.DrawSubset(
					m_Subsets[i].IndexNum,
					m_Subsets[i].IndexBase,
					m_Subsets[i].VertexBase);
			}
		}

		// グロー（本体より大きく、光だけを足して描く）
		//{
		//	Renderer::SetBlendState(BS_ADDITIVE);
		//	Matrix world = Matrix::CreateScale(dx.scale * 1.6f) * Matrix::CreateTranslation(dx.pos);	// ← pos を dx.pos に
		//	Renderer::SetWorldMatrix(&world);

		//	for (size_t i = 0; i < m_Subsets.size(); i++)
		//	{
		//		m_Materials[m_Subsets[i].MaterialIdx]->SetDiffuse(
		//			DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));	// 光源反射なし
		//		m_Materials[m_Subsets[i].MaterialIdx]->SetEmission(
		//			DirectX::XMFLOAT4(dx.color.x * 0.5f, dx.color.y * 0.5f, dx.color.z * 0.5f, 1.0f));	// ← laneColor を dx.color に
		//		m_Materials[m_Subsets[i].MaterialIdx]->Update();
		//		m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();
		//		m_MeshRenderer.DrawSubset(
		//			m_Subsets[i].IndexNum,
		//			m_Subsets[i].IndexBase,
		//			m_Subsets[i].VertexBase);
		//	}
		//	Renderer::SetBlendState(BS_NONE);
		//}
	}

	Renderer::SetBlendState(BS_ADDITIVE);
	for (auto& p : m_TapAuraParticles)
		m_TapAuraParticleQuad.Draw(cam, p.GetPosition(), p.GetScale(), p.GetColor());
	Renderer::SetBlendState(BS_NONE);


	// ── ロングノーツ・フリックノーツの描画 ──
	if (kHighwayMode) DrawHighwayLongNotes(cam);
	else DrawLongNotes(cam);
	DrawFlickNotes(cam);

	//魔法障壁　ロングノーツ成功中に出すやつ
	{
		bool holding[6] = { false,false,false,false,false,false };
		for (auto& note : m_Notes)
		{
			if (!note.IsLong()) continue;
			if (note.judged) continue;
			if (note.lane < 0 || note.lane >= 6) continue;
			if (!IsHoldingLong(note)) continue;
			holding[note.LaneAt(m_MusicTime)] = true;
		}
		constexpr float kBarrierScale = 260.0f;	//大きさ
		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int lane = 0; lane < 6; lane++)
		{
			if (!holding[lane]) continue;
			Vector3 pos = m_JudgeCircles[lane].GetPosition(m_MusicTime);
			Vector3 laneColor = GetLaneColor(lane);
			float pulse = 0.75f + 0.15f * sinf(m_MusicTime * 6.0f);
			m_BarrierPlate.Draw(pos, kBarrierScale, pulse, laneColor);
		}
		Renderer::SetBlendState(BS_NONE);
	}

	// ── 接近リング（判定ラインに近づくにつれて大きく濃くなる）──
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (note.IsFlick()) continue;

		float st = note.GetSpawnTime();
		// LONGは先端が到達するarrivalTimeを終点とする。TAPは通常のtime
		float et = note.IsLong() ? note.arrivalTime : note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		float duration = et - st;
		float t = (duration > 0.0001f) ? (m_MusicTime - st) / duration : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		Vector3 pos = note.IsLong()
			? NotePath::EvaluateLongNotePosition(note, m_MusicTime)
			: NotePath::EvaluateNotePosition(note, m_MusicTime);

		float ringScale = 100.0f + t * 60.0f;
		float ringAlpha = 0.2f + t * 0.6f;
		m_ApproachRing.Draw(pos, ringScale, ringAlpha);
	}
	Renderer::SetBlendState(BS_NONE);

	// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──
	// （ここで計算した座標は、後段のComboHud::Draw呼び出しでも再利用する）
	Vector3 judgeCirclePos3D[6];
	Vector3 judgeCircleScreenPos[6];
	for (int lane = 0; lane < 6; lane++)
	{
		judgeCirclePos3D[lane] = m_JudgeCircles[lane].GetPosition(m_MusicTime);
		judgeCircleScreenPos[lane] = WorldToTexture2DPos(judgeCirclePos3D[lane], view);
	}

		Renderer::SetBlendState(BS_NONE);

	// ── パーティクル（判定ヒット時の花びら演出）──
	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (auto& p : m_Particle)
	{
		m_ParticleQuad.Draw(cam, p.GetPosition(), p.GetScale(), p.GetColor());
	}
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetBlendState(BS_ADDITIVE);
	for (auto& p : m_LongAmbientParticles)
	{
		m_LongParticleQuad.Draw(cam, p.GetPosition(), p.GetScale(), p.GetColor());
	}

	// ── コンボ表示・判定ランク演出・判定内訳・障壁演出は ComboHud にまとめて委譲 ──
	Vector3 laneColors[6];
	for (int i = 0; i < 6; i++) laneColors[i] = GetLaneColor(i);

	m_ComboHud.Draw(cam, view,
		m_JudgeSystem.GetComboCount(),
		m_JudgeSystem.GetPerfectCount(), m_JudgeSystem.GetGreatCount(),
		m_JudgeSystem.GetGoodCount(), m_JudgeSystem.GetMissCount(),
		judgeCircleScreenPos, judgeCirclePos3D, laneColors);

	//ステージの初めに左から順にイーズイン
	if (m_Started) m_keyt.Draw(cam, m_MusicTime + m_CountInTime, judgeCircleScreenPos);

	DrawLightEffects(cam);
	// ── ホーミングビームの描画（他要素の最後に描くことで前面に出す）──
	{
		int playerSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		m_BeamSystem.Draw(cam, m_MusicTime, m_EnemyRef, GetPlayerSideAnchor(playerSide));
	}

	//戦闘の演出：HUDより先に描いて上に来るように
	m_LongShield.Draw(cam);
	m_WeakPoint.Draw(cam);
	m_AttackOrbs.Draw(cam);
	m_SpecialBeam.Draw(cam);

	m_ImpactFx.Draw(cam);
	m_EnemyHp.Draw(cam);
	m_SpGauge.Draw(cam);
	m_ScoreDisplay.Draw(cam);
	cam->SetCamera(0);
}

// ============================================================
// Uninit : 終了処理。現状、明示的な解放が必要なリソースは無い
// （メンバのunique_ptr/vectorがデストラクタで自動解放される）。
// ============================================================
void ChartObject::Uninit()
{
	sound.StopDynamic();
	StopLongSounds();
	sound.Uninit();
}
