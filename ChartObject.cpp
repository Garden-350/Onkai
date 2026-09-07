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
	// レーンごとの左右補正量（S,D,F,J,K,L の順）
// S/L ペア、D/K ペア、F/J ペアで同じ値
	constexpr float kLaneOffsetFix[6] = {
		180.0f,  // S (lane0)
		110.0f,  // D (lane1)
		40.0f,  // F (lane2)
		40.0f,  // J (lane3)
		110.0f,  // K (lane4)
		180.0f,  // L (lane5)
	};
	// 2つのVector3を線形補間する
	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return a + (b - a) * t;
	}

	// レーン(0〜5) <-> S/D/F/J/K/Lキーの対応表
	constexpr int kLaneKeys[6] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };

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

// ============================================================
// Init : オブジェクト初期化。
//   各種3Dモデル・テクスチャ・判定円のデフォルト値をセットアップする。
//   実際の譜面データは LoadChart で別途読み込む。
// ============================================================
void ChartObject::Init()
{

	m_AutoMode = Game::GetInstance()->IsAutoMode();
	m_JudgeSystem.SetOnJudge([this](int laneIndex, int rank) { OnJudge(laneIndex, rank); });
	m_JudgeSystem.SetOnEmptyPress([this](int laneIndex) { OnEmptyLanePress(laneIndex); });
	// サウンドエンジン初期化 
	sound.Init();

	//  TAPノーツ本体モデルのロード（これは必須。無いと何も表示されない）
	StaticMesh staticmesh;
	std::string modelFile = "assets/model/maru/maru.fbx";
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

	// ── ロングノーツ：先端(sentan)モデルのロード ──
	// エディター側の kusari_sentan_2.fbx に相当するアセットを想定。
	// 無ければ m_LongSentanLoaded=false のままとなり、描画側でTAPモデルにフォールバックする。
	m_LongSentanLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/kusari/kusari_sentan_2.fbx", "assets/model/long_sentan",
		m_LongSentanRenderer, m_LongSentanShader, m_LongSentanSubsets, m_LongSentanMaterials, m_LongSentanTextures);
	if (!m_LongSentanLoaded)
		OutputDebugStringA("[ChartObject] Long sentan model not found. Fallback to TAP model.\n");

	// ── ロングノーツ：本体(dou/鎖)モデルのロード ──
	// エディター側の kusari_dou.fbx に相当。押している間、先端〜終端の間に繰り返し並べる。
	m_LongDouLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/kusari/kusari_dou.fbx", "assets/model/long_dou",
		m_LongDouRenderer, m_LongDouShader, m_LongDouSubsets, m_LongDouMaterials, m_LongDouTextures);
	if (!m_LongDouLoaded)
		OutputDebugStringA("[ChartObject] Long dou model not found. Fallback to TAP model.\n");

	// ── ロングノーツ：終端(o/輪)モデルのロード ──
	// エディター側の kusari_o_2.fbx に相当。先端よりholdDuration秒遅れて追従する。
	m_LongOLoaded = ModelUtil::LoadOptionalModel(
		"assets/model/kusari/kusari_o_2.fbx", "assets/model/long_o",
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
		key.y = -50.0f;     // -55→-20 (プレイヤーとのY差を少し詰める)
		key.z = 300.0f;     // 150→300 (カメラから適度に離す)
		m_JudgeCircles[i].keyframe.push_back(key);
	}

	// ── 接近リング・パーティクルの画像セット ──
	m_ApproachRing.Init("assets/texture/ring.png");
	m_ParticleQuad.Init("assets/texture/sakura2.png");
	m_MagicCircleQuad.Init("assets/texture/magic-circle (1).png");

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
	m_JudgeCircles = result.judgeCircles;
	m_BeamSystem.SetBeams(result.homingBeams);
	m_Bpm = result.bpm;
	m_BeatsPerBar = result.beatsPerBar;
	m_AudioFile = result.audioFile;

	// ── 音声ファイルの読み込み ──
	if (!m_AudioFile.empty())
	{
		std::string fullPath = "assets/bgm/" + m_AudioFile; //曲を読み込んだ際は絶対にエディターで使ってる曲を入れること
		HRESULT hr = sound.LoadDynamic(fullPath, true);
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

	OutputDebugStringA(("[ChartPlayObject] Loaded: charts/" + name + ".json notes=" +
		std::to_string(m_Notes.size()) + " beams=" + std::to_string(m_BeamSystem.GetBeamCount()) + "\n").c_str());
	return true;
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
// EvaluateNotePosition / EvaluateLongNotePosition は NotePath.h/.cpp に切り出し済み
// ============================================================

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

	m_ComboHud.Update(deltaTime);

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
	m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);

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

	if (m_AutoMode)
	{
		m_JudgeSystem.UpdateAutoJudgge(m_Notes, m_MusicTime, sound);
	}
	else
	{
		//TAPノーツの自動ミス判定
		m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);

		int playerSideForFlick = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		for (int lane = 0; lane < 6; lane++)
		{
			m_JudgeSystem.JudgeLane(kLaneKeys[lane], lane, m_Notes, m_MusicTime, sound);
		}

		// フリック・ロングノーツの判定
		m_JudgeSystem.JudgeFlickNotes(m_Notes, m_MusicTime, playerSideForFlick);
		m_JudgeSystem.JudgeLongNoteHold(m_Notes, m_MusicTime);
	}

	// 判定済み(judged=true)になったノーツをまとめて削除
	// （TAPの判定はJudgeLaneで即座にjudged=trueにするが、実削除はここでまとめて行う）
	m_Notes.erase(
		std::remove_if(m_Notes.begin(), m_Notes.end(),
			[](const ChartNote& n) { return n.judged; }),
		m_Notes.end());

	UpdateLightEffects(deltaTime);
	// ホーミングビームの状態遷移とプレイヤー移動の更新
	{
		int playerSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		m_BeamSystem.Update(deltaTime, m_MusicTime, playerSide, GetPlayerSideAnchor(playerSide), sound, m_ScreenEffects);
	}
}

// InitBeamScatter は ChartLoader.cpp のローカル関数に切り出し済み

// UpdateHomingBeams は HomingBeamSystem::Update に切り出し済み

// ============================================================
// OnJudge : 判定がヒットした瞬間に呼ばれるフック。
//   コンボを加算し、判定円の位置に花びらパーティクルを100個生成する。
// ============================================================
void ChartObject::OnJudge(int laneIndex, int rank)
{
	m_ComboHud.TriggerBarrier(laneIndex);
	m_ComboHud.TriggerRankPop(laneIndex, rank);
	m_ComboHud.TriggerComboPop();

	if (laneIndex < 0 || laneIndex >= 6) return;
	Vector3 pos = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	for (int i = 0; i < 100; i++)
	{
		float deg = (float)(rand() % 360);
		Particle p(
			pos,
			1.5f,                          // 寿命
			50.0f,                         // 重力
			Color(1, 1, 1, 1),				// 開始色
			Color(1, 1, 1, 0),				// 終了色（透明にフェード）
			0.7f,                          // フェード開始位置（終盤30%だけフェード）
			80.0f,                         // 開始サイズ
			40.0f,                         // 終了サイズ
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

	le.origin.x += (laneIndex < 3) ? -kLaneOffsetFix[laneIndex] : kLaneOffsetFix[laneIndex];

	m_LightEffects.push_back(le);
	(void)rank; // 現状rank別の演出差はまだ未実装。将来ランクごとに色や量を変える場合はここで分岐する
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

	le.origin.x += (laneIndex < 3) ? -kLaneOffsetFix[laneIndex] : kLaneOffsetFix[laneIndex];

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

	Renderer::SetBlendState(BS_ADDITIVE);

	constexpr int kSegments = 6;
	constexpr float kMaxDistance = 500.0f;	//物理的な長さ

	Matrix view = cam->GetViewMatrix();
	Matrix invView = view.Invert();
	Vector3 screenUpInWorld(invView._21, invView._22, invView._23);
	screenUpInWorld.Normalize();

	for (auto& le : m_LightEffects)
	{
		float age = m_MusicTime - le.startTime;
		float t = std::clamp(age / le.duration, 0.0f, 1.0f);

		// ── アニメーションカーブ ──
		// 立ち上がりは一瞬(0〜0.15)でMAXに達し、そこから減衰していく
		float growT = std::clamp(t / 0.15f, 0.0f, 1.0f);
		float riseEase = 1.0f - (1.0f - growT) * (1.0f - growT); // ease-out
		float decay = 1.0f - t;                                  // 線形フェード
		decay = decay * decay;                                    // 後半を急速に消す(ease-in)

		float envelope = riseEase * decay; // 全体の強さ(0〜1)
		if (envelope <= 0.001f) continue;

		Vector3 from = le.origin;
		Vector3 dir = screenUpInWorld;
		Vector3 color = GetLaneColor(le.lane);

		float rootScale = 120.0f * envelope;
		m_ParticleQuad.Draw(cam, from, rootScale,
			Color(color.x, color.y, color.z, envelope));

		// 根本→先端で幅が広がる台形。envelopeで全体の伸び具合も変える
		float reach = kMaxDistance * (0.3f + 0.7f * riseEase); // 出た瞬間ニュッと伸びる

		for (int s = 0; s < kSegments; s++)
		{
			float p0 = (float)s / kSegments;
			float p1 = (float)(s + 1) / kSegments;

			Vector3 segFrom = from + dir * (reach * p0);
			Vector3 segTo = from + dir * (reach * p1);

			float baseWidth = 200.0f + 20.0f * p1; // 先端に向けて広がる

			// ── 芯(コア)：細く強い ──
			m_BeamSystem.DrawBeamSegmentWithColor(cam, segFrom, segTo,
				baseWidth * 0.35f, 4.0f, color, le.intensity * 1.6f * envelope);

			// ── グロー：太く柔らかい ──
			m_BeamSystem.DrawBeamSegmentWithColor(cam, segFrom, segTo,
				baseWidth, 10.0f, color, le.intensity * 0.5f * envelope);
		}
	}

	Renderer::SetBlendState(BS_NONE);
}

// DrawBeamSegment / DrawBeamSegmentWithColor / DrawJudggedBeam / DrawHomingBeams は HomingBeamSystem に切り出し済み

// ============================================================
// DrawLongNotes : ロングノーツを sentan(先端) / dou(本体・鎖) / o(終端の輪) の
//                 3パーツで描画する。
//
//   時間の流れ：
//     displayStart(出現) → arrivalTime(先端が判定ラインに到達) → endTime(終端が到達し消える)
//
//   sentanは arrivalTime を過ぎても kGraceTime 秒だけ進み続け、
//   oはholdDuration秒だけ遅れてsentanを追いかける。
//   douはその間を一定間隔でサンプリングして敷き詰める（鎖のように見せる）。
// ============================================================
void ChartObject::DrawLongNotes(Camera* cam)
{
	constexpr float kGraceTime = 0.5f;		// 判定ラインを越えても先端が進み続ける猶予秒数
	constexpr float kBodySpacing = 0.02f;	// dou（本体）を敷き詰める間隔（秒）。小さいほど密になるが重くなる
	constexpr int   kMaxBodySegments = 60;	// dou描画数の安全上限（負荷対策）

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

		// 出現〜到達が短すぎる（≒0秒）場合は0除算やワープ表示を避けるためスキップ
		if (arrivalTime - displayStart <= 0.0001f) continue;
		if (holdDuration <= 0.0001f) holdDuration = 0.0001f;

		// 終端が消えるまで（猶予込み）の間だけ表示対象とする
		float extendedEnd = endTime + kGraceTime;
		if (m_MusicTime < displayStart || m_MusicTime > extendedEnd) continue;

		// ── 1) sentan（先端）の現在位置 ──
		// arrivalTimeを過ぎてもkGraceTime秒だけ進み続ける（急に消えないようにするため）
		float headTime = std::min(m_MusicTime, endTime);
		Vector3 headPos = NotePath::EvaluateLongNotePosition(note, headTime);
		{
			Matrix world = Matrix::CreateScale(20.0f) * Matrix::CreateTranslation(headPos);
			ModelUtil::DrawModel(sentanRenderer, sentanShader, sentanSubsets, sentanMaterials, sentanTextures, world);
		}

		// ── 3) o（終端の輪）の現在位置 ──
		// sentanよりholdDuration秒だけ遅れて追従する（まだ出現範囲に入っていなければ非表示）
		float tailTime = headTime - holdDuration;
		bool tailVisible = (tailTime >= displayStart);
		if (tailVisible)
		{
			Vector3 tailPos = NotePath::EvaluateLongNotePosition(note, tailTime);
			Matrix world = Matrix::CreateScale(20.0f) * Matrix::CreateTranslation(tailPos);
			ModelUtil::DrawModel(oRenderer, oShader, oSubsets, oMaterials, oTextures, world);
		}

		// ── 2) dou（本体・鎖）の描画 ──
		// tailTime〜headTimeの区間を一定間隔でサンプリングし、間を鎖状に敷き詰める
		{
			float bodyStart = tailVisible ? tailTime : displayStart;
			float bodyEnd = headTime;
			float bodyDuration = bodyEnd - bodyStart;
			if (bodyDuration > 0.0001f)
			{
				int segments = std::max(2, (int)std::ceil(bodyDuration / kBodySpacing));
				segments = std::min(segments, kMaxBodySegments); // 描画数に上限をかけて負荷を抑える

				for (int s = 1; s < segments; s++) // 両端(sentan/o)は上ですでに描画済みなので中間だけ
				{
					float p = (float)s / (float)segments;
					float sampleTime = bodyStart + bodyDuration * p;
					Vector3 pos = NotePath::EvaluateLongNotePosition(note, sampleTime);
					Matrix world = Matrix::CreateScale(10.0f) * Matrix::CreateTranslation(pos);
					ModelUtil::DrawModel(douRenderer, douShader, douSubsets, douMaterials, douTextures, world);
				}
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
		float rotY = note.IsFlickRight() ? 0.0f : DirectX::XM_PI;

		Matrix world =
			Matrix::CreateScale(m_NoteScale * 0.7f) *
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

	// ── TAPノーツ本体の描画 ──
	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();

	for (auto& note : m_Notes)
	{
		if (!note.IsTap()) continue;
		if (note.judged) continue;

		float st = note.GetSpawnTime();
		float et = note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		Vector3 pos = NotePath::EvaluateNotePosition(note, m_MusicTime);
		Vector3 laneColor = GetLaneColor(note.lane);
		{
			Matrix world = Matrix::CreateScale(m_NoteScale) * Matrix::CreateTranslation(pos);
			Renderer::SetWorldMatrix(&world);

			for (size_t i = 0; i < m_Subsets.size(); i++)
			{
				m_Materials[m_Subsets[i].MaterialIdx]->SetDiffuse(
					DirectX::XMFLOAT4(laneColor.x, laneColor.y, laneColor.z, 1.0f)
				);
				m_Materials[m_Subsets[i].MaterialIdx]->SetEmission(
					DirectX::XMFLOAT4(laneColor.z, laneColor.y, laneColor.z, 1.0f)
				);
				m_Materials[m_Subsets[i].MaterialIdx]->Update();

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

		//Matrix s = Matrix::CreateScale(m_NoteScale);
		//Matrix t = Matrix::CreateTranslation(pos);
		//Matrix world = s * t;
		//Renderer::SetWorldMatrix(&world);


		//グロー処理
		{
			Renderer::SetBlendState(BS_ADDITIVE);
			Matrix world = Matrix::CreateScale(m_NoteScale * 1.6f) * Matrix::CreateTranslation(pos);
			Renderer::SetWorldMatrix(&world);

			for (size_t i = 0; i < m_Subsets.size(); i++)
			{
				m_Materials[m_Subsets[i].MaterialIdx]->SetDiffuse(
					DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));	//光源反射なし
				m_Materials[m_Subsets[i].MaterialIdx]->SetEmission(
					DirectX::XMFLOAT4(laneColor.x * 0.5f, laneColor.y * 0.5f, laneColor.z * 0.5f, 1.0f));
				m_Materials[m_Subsets[i].MaterialIdx]->Update();
				m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();
				m_MeshRenderer.DrawSubset(
					m_Subsets[i].IndexNum,
					m_Subsets[i].IndexBase,
					m_Subsets[i].VertexBase
				);
			}
			Renderer::SetBlendState(BS_NONE);
		}
	}


	// ── ロングノーツ・フリックノーツの描画 ──
	DrawLongNotes(cam);
	DrawFlickNotes(cam);

	// ── 接近リング（判定ラインに近づくにつれて大きく濃くなる）──
	// フリックは対象外（エディタと同じ扱い）
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

	// ── レーンごとに一番近い未判定ノーツの接近度(0〜1)を計算 ──
	// （判定円ガイドの「育つ四角」の大きさに使う。フリックは対象外）
	float laneT[6] = { 0,0,0,0,0,0 };
	bool laneActive[6] = { false,false,false,false,false,false };
	const ChartNote* laneActiveNote[6] = { nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (note.IsFlick()) continue;
		if (note.lane < 0 || note.lane >= 6) continue;

		float st = note.GetSpawnTime();
		float et = note.IsLong() ? note.arrivalTime : note.time;
		if (m_MusicTime < st || m_MusicTime > et) continue;

		float duration = et - st;
		float t = (duration > 0.00001f) ? (m_MusicTime - st) / duration : 1.0f;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		// そのレーンで最も判定に近い(t最大)ノーツを採用
		if (!laneActive[note.lane] || t > laneT[note.lane])
		{
			laneActive[note.lane] = true;
			laneT[note.lane] = t;
			laneActiveNote[note.lane] = &note;
		}
	}

	// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──

	// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──
	// （ここで計算した座標は、後段のComboHud::Draw呼び出しでも再利用する）
	Vector3 judgeCirclePos3D[6];
	Vector3 judgeCircleScreenPos[6];

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		Vector3 circlePos3D = m_JudgeCircles[i].GetPosition(m_MusicTime);
		Vector3 screenPos = WorldToTexture2DPos(circlePos3D, view);
		judgeCirclePos3D[i] = circlePos3D;
		judgeCircleScreenPos[i] = screenPos;

		if (m_ShowLaneKeyIcons)
		{
			m_LaneIcons[i].SetPosition(screenPos);
			m_LaneIcons[i].Draw(cam);
		}

		Renderer::SetBlendState(BS_ALPHABLEND);
		if (m_ShowLaneFrame)
		{
			m_LaneFrame[i].SetPosition(screenPos);
			m_LaneFrame[i].Draw(cam);
		}
		if (laneActive[i])
		{
			constexpr float kFillMinSize = 10.0f;
			float fillSize = kFillMinSize + laneT[i] * (m_FrameSize - kFillMinSize);
			m_LaneFill[i].SetScale(fillSize, fillSize, 1.0f);
			m_LaneFill[i].SetPosition(screenPos);

			// ── 判定円のフェードイン：出現時は透明、判定時刻ちょうどで完全に見える ──
			float fillAlpha = laneT[i] * laneT[i] * laneT[i]; // 三乗カーブ：終盤で一気にはっきり見える
			m_LaneFill[i].SetAlpha(fillAlpha);

			// フィナーレ演出：spiralTurns付きノーツに追従して判定円自体も回転させる
			if (laneActiveNote[i] && laneActiveNote[i]->spiralTurns != 0.0f)
			{
				float spinDeg = laneT[i] * laneActiveNote[i]->spiralTurns * 360.0f;
				m_LaneFill[i].SetRotation(0.0f, 0.0f, spinDeg);
			}
			else
			{
				m_LaneFill[i].SetRotation(0.0f, 0.0f, 0.0f);
			}

			m_LaneFill[i].Draw(cam);
		}
		//if (laneActive[i])
		//{
		//	float fillSize = 10.0f + laneT[i] * 90.0f;
		//	m_LaneFill[i].SetScale(fillSize, fillSize, 1.0f);
		//	m_LaneFill[i].SetPosition(screenPos);
		//	m_LaneFill[i].Draw(cam);
		//}
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

	// ── コンボ表示・判定ランク演出・判定内訳・障壁演出は ComboHud にまとめて委譲 ──
	Vector3 laneColors[6];
	for (int i = 0; i < 6; i++) laneColors[i] = GetLaneColor(i);

	m_ComboHud.Draw(cam, view,
		m_JudgeSystem.GetComboCount(),
		m_JudgeSystem.GetPerfectCount(), m_JudgeSystem.GetGreatCount(),
		m_JudgeSystem.GetGoodCount(), m_JudgeSystem.GetMissCount(),
		judgeCircleScreenPos, judgeCirclePos3D, laneColors);

	DrawLightEffects(cam);
	// ── ホーミングビームの描画（他要素の最後に描くことで前面に出す）──
	{
		int playerSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		m_BeamSystem.Draw(cam, m_MusicTime, m_EnemyRef, GetPlayerSideAnchor(playerSide));
	}

	cam->SetCamera(0);
}

// ============================================================
// Uninit : 終了処理。現状、明示的な解放が必要なリソースは無い
// （メンバのunique_ptr/vectorがデストラクタで自動解放される）。
// ============================================================
void ChartObject::Uninit()
{
	sound.StopDynamic();
}