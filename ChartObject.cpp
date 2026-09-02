#include "ChartObject.h"
#include "SimpleJson.h"
#include "Renderer.h"
#include "Camera.h"
#include "Input.h"
#include "Game.h"
#include "Player.h"
#include "Application.h"
#include "sound.h"
#include "Enemy.h"
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
	constexpr float kXScale = 1.7f;	//540にこの値を掛ける
	constexpr float kEditorRawCenterX = -85.0f;
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

// ============================================================
// LoadOptionalModel : 「無くても構わない」補助モデルの読み込み共通処理。
//   ロード失敗してもfalseを返すだけで、呼び出し側はTAPモデル等に
//   フォールバックできるようにするためのヘルパー。
//
//   注意：StaticMesh::Load は戻り値がvoidの実装のため、
//         ロード成功可否は「サブセットが1つ以上あるか」で簡易判定している。
// ============================================================
static bool LoadOptionalModel(
	const char* modelPath, const char* texDir,
	MeshRenderer& renderer, Shader& shader,
	std::vector<SUBSET>& subsets,
	std::vector<std::unique_ptr<Material>>& materials,
	std::vector<std::unique_ptr<Texture>>& textures)
{
	StaticMesh mesh;
	mesh.Load(modelPath, texDir);

	// サブセットが空 = 実質ロード失敗とみなす
	if (mesh.GetSubsets().empty())
	{
		OutputDebugStringA("[LoadOptionalModel] Failed to load mesh\n");
		return false;
	}

	renderer.Init(mesh);
	shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");
	subsets = mesh.GetSubsets();
	textures = mesh.GetTextures();

	std::vector<MATERIAL> mats = mesh.GetMaterials();
	for (auto& m : mats)
	{
		auto mat = std::make_unique<Material>();
		mat->Create(m);
		materials.push_back(std::move(mat));
	}
	return true;
}

// ============================================================
// DrawModel : 指定したモデル一式(subsets/materials/textures)を
//             指定ワールド行列で1回描画する共通処理。
//
//   Renderer::SetWorldMatrix が非constの Matrix* を要求するため、
//   const参照で受け取った行列を一度ローカル変数へコピーしてから渡している。
// ============================================================
static void DrawModel(MeshRenderer& renderer, Shader& shader,
	std::vector<SUBSET>& subsets,
	std::vector<std::unique_ptr<Material>>& materials,
	std::vector<std::unique_ptr<Texture>>& textures,
	const Matrix& worldConst)
{
	Matrix world = worldConst; // const→非constコピー
	Renderer::SetWorldMatrix(&world);
	shader.SetGPU();
	renderer.BeforeDraw();

	for (size_t i = 0; i < subsets.size(); i++)
	{
		materials[subsets[i].MaterialIdx]->SetGPU();
		if (materials[subsets[i].MaterialIdx]->isTextureEnable())
			textures[subsets[i].MaterialIdx]->SetGPU();

		renderer.DrawSubset(
			subsets[i].IndexNum,
			subsets[i].IndexBase,
			subsets[i].VertexBase
		);
	}
}

// ============================================================
// Init : オブジェクト初期化。
//   各種3Dモデル・テクスチャ・判定円のデフォルト値をセットアップする。
//   実際の譜面データは LoadChart で別途読み込む。
// ============================================================
void ChartObject::Init()
{
	m_AutoMode = Game::GetInstance()->IsAutoMode();
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
	m_LongSentanLoaded = LoadOptionalModel(
		"assets/model/kusari/kusari_sentan_2.fbx", "assets/model/long_sentan",
		m_LongSentanRenderer, m_LongSentanShader, m_LongSentanSubsets, m_LongSentanMaterials, m_LongSentanTextures);
	if (!m_LongSentanLoaded)
		OutputDebugStringA("[ChartObject] Long sentan model not found. Fallback to TAP model.\n");

	// ── ロングノーツ：本体(dou/鎖)モデルのロード ──
	// エディター側の kusari_dou.fbx に相当。押している間、先端〜終端の間に繰り返し並べる。
	m_LongDouLoaded = LoadOptionalModel(
		"assets/model/kusari/kusari_dou.fbx", "assets/model/long_dou",
		m_LongDouRenderer, m_LongDouShader, m_LongDouSubsets, m_LongDouMaterials, m_LongDouTextures);
	if (!m_LongDouLoaded)
		OutputDebugStringA("[ChartObject] Long dou model not found. Fallback to TAP model.\n");

	// ── ロングノーツ：終端(o/輪)モデルのロード ──
	// エディター側の kusari_o_2.fbx に相当。先端よりholdDuration秒遅れて追従する。
	m_LongOLoaded = LoadOptionalModel(
		"assets/model/kusari/kusari_o_2.fbx", "assets/model/long_o",
		m_LongORenderer, m_LongOShader, m_LongOSubsets, m_LongOMaterials, m_LongOTextures);
	if (!m_LongOLoaded)
		OutputDebugStringA("[ChartObject] Long o model not found. Fallback to TAP model.\n");

	// ── フリック用モデルのロード ──
	m_FlickModelLoaded = LoadOptionalModel(
		"assets/model/Flick/allow.fbx", "assets/model/flick",
		m_FlickMeshRenderer, m_FlickShader, m_FlickSubsets, m_FlickMaterials, m_FlickTextures);
	if (!m_FlickModelLoaded)
		OutputDebugStringA("[ChartObject] Flick model not found. Fallback to TAP model.\n");

	// ── ビーム用モデルのロード ──
	// 棒状のFBXを想定。無い場合、ビームは一切描画されない（DrawHomingBeams内でreturnする）。
	m_BeamModelLoaded = LoadOptionalModel(
		"assets/model/Beam/beam.fbx", "assets/model/beam",
		m_BeamMeshRenderer, m_BeamShader, m_BeamSubsets, m_BeamMaterials, m_BeamTextures);
	if (!m_BeamModelLoaded)
		OutputDebugStringA("[ChartObject] Beam model not found. Beam will not be drawn.\n");
	if (m_BeamModelLoaded)
	{
		m_BeamShader.Create("shader/litTextureVS.hlsl", "shader/beamPS.hlsl");
		m_BeamShader.CreatePSConstantBuffer(sizeof(BeamShaderParms));

		if (!m_BeamNoiseTexture.Load("assets/texture/noise.png"))
		{
			std::cout << ("[ChartObject] Beam noise texture not found.\n");
		}
	}

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

	// ── コンボ数字(4桁分)のセットアップ ──
	for (int i = 0; i < 4; i++)
	{
		m_ComboDigits[i].Init();
		m_ComboDigits[i].SetTexture("assets/texture/allnumber_new.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(m_NuberSplite_Size, m_NuberSplite_Size, 1.0f);
	}

	auto initDigitRow = [this](Texture2D* row)
		{
			for (int i = 0; i < 4; i++)
			{
				row[i].Init();
				row[i].SetTexture("assets/texture/allnumber_new.png");
				row[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
				row[i].SetScale(m_NuberSplite_Size * 0.6f, m_NuberSplite_Size * 0.6f, 1.0f); // コンボより小さめ
			}
		};
	initDigitRow(m_PerfectDigits);
	initDigitRow(m_GreatDigits);
	initDigitRow(m_GoodDigits);
	initDigitRow(m_MissDigits);

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

	m_JudgeRankPerfect.Init();
	m_JudgeRankPerfect.SetTexture("assets/texture/PERFECT.png");
	m_JudgeRankPerfect.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGreat.Init();
	m_JudgeRankGreat.SetTexture("assets/texture/GREAT.png");
	m_JudgeRankGreat.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGood.Init();
	m_JudgeRankGood.SetTexture("assets/texture/GOOD.png");
	m_JudgeRankGood.SetScale(200.0f, 50.0f, 1.0f);

	m_BarrierQuad.Init();
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
	JsonValue root;
	std::string err;
	if (!SimpleJson::ParseFile("charts/" + name + ".json", root, &err))
	{
		OutputDebugStringA(("[ChartObject] Load failed: " + err + "\n").c_str());
		return false;
	}

	// ── メタ情報（bpm/beatsPerBarはトップ直下 or "meta"の下のどちらにも対応）──
	if (root.Has("bpm")) m_Bpm = root["bpm"].AsFloat(m_Bpm);
	else if (root.Has("meta") && root["meta"].Has("bpm")) m_Bpm = root["meta"]["bpm"].AsFloat(m_Bpm);

	if (root.Has("beatsPerBar")) m_BeatsPerBar = root["beatsPerBar"].AsInt(m_BeatsPerBar);
	else if (root.Has("meta") && root["meta"].Has("beatsPerBar")) m_BeatsPerBar = root["meta"]["beatsPerBar"].AsInt(m_BeatsPerBar);

	// ── 音声ファイル名の読み込み ──
	if (root.Has("audioFile"))
		m_AudioFile = root["audioFile"].AsString("");
	else if (root.Has("meta") && root["meta"].Has("audioFile"))
		m_AudioFile = root["meta"]["audioFile"].AsString("");
	else
		m_AudioFile = "";

	if (!m_AudioFile.empty())
	{
		std::string fullPath = "assets/bgm/" + m_AudioFile; //曲を読み込んだ際は絶対にエディターで使ってる曲を入れること
		HRESULT hr = sound.LoadDynamic(fullPath, true);
		if (FAILED(hr))
			OutputDebugStringA(("[ChartObject] Failed to load audio: " + fullPath + "\n").c_str());
	}
	m_Notes.clear();
	m_HomingBeams.clear();

	// ── メトロノームのカウントイン準備（1小節分）──
	float secondsPerBeatForCue = (m_Bpm > 0.0f) ? 60.0f / m_Bpm : 0.5f;
	m_CountInTime = secondsPerBeatForCue * m_BeatsPerBar;
	m_CountInNextBeat = 0;

	// ── ノーツ読み込み ──
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

			// arrivalTime（ロングノーツの先端到達時刻。無ければtimeと同じにしておく）
			if (n.Has("arrivalTime"))
				note.arrivalTime = n["arrivalTime"].AsFloat(note.time);
			else
				note.arrivalTime = note.time;

			// フリック終点レーン（無ければ未設定=-1）
			if (n.Has("flickLaneTo"))
				note.flickLaneTo = n["flickLaneTo"].AsInt(-1);
			else
				note.flickLaneTo = -1;

			// ロングノーツの保持時間（無ければ arrivalTime と time の差から逆算）
			if (n.Has("holdDuration"))
				note.holdDuration = n["holdDuration"].AsFloat(0.0f);
			else
				note.holdDuration = std::max(0.0f, note.time - note.arrivalTime);

			// 出現位置（無ければレーン番号から自動配置）
			if (n.Has("spawnPos"))
			{
				const auto& sp = n["spawnPos"];
				note.spawnPos = Vector3(-sp["x"].AsFloat(), sp["y"].AsFloat(), sp["z"].AsFloat());
			}
			else
			{
				float laneX = -310.0f + (1660.0f / 6.0f) * note.lane + (1660.0f / 12.0f);
				note.spawnPos = Vector3(-laneX, -55.0f, -210.0f);
			}

			// 判定位置（無ければレーン番号から自動配置）
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

			// ベジェ曲線の制御点（あれば）
			if (n.Has("path") && n["path"].IsArray())
			{
				for (const auto& p : n["path"].arrayValue)
				{
					note.controlPoints.emplace_back(
						-p["x"].AsFloat(), p["y"].AsFloat(), p["z"].AsFloat()
					);
				}
			}

			// 状態フラグの初期化
			note.judged = false;
			note.approcachPlayed = false;
			note.longStarted = false;
			note.longFailed = false;

			m_Notes.push_back(note);
		}
		// 判定時刻の昇順にソート（描画・判定処理を楽にするため）
		std::sort(m_Notes.begin(), m_Notes.end(),
			[](const ChartNote& a, const ChartNote& b) {return a.time < b.time; });
	}

	// ── 判定円のキーフレーム読み込み ──
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
					float raw = k["x"].AsFloat();
					float spread = raw - kEditorRawCenterX;              // 中心からのズレ量だけ取り出す
					key.x = -(kEditorRawCenterX + spread * kXScale);     // 中心は固定、広がりだけ拡大

					key.y = k["y"].AsFloat();
					key.z = k["z"].AsFloat();
					circle.keyframe.push_back(key);
				}
			}
			// 時刻順にソート
			std::sort(circle.keyframe.begin(), circle.keyframe.end(),
				[](const ChartCircleKeyframe& a, const ChartCircleKeyframe& b) {
					return a.time < b.time;
				});
		}
	}

	// ── ホーミングビームの読み込み ──
	if (root.Has("homingBeams") && root["homingBeams"].IsArray())
	{
		for (const auto& b : root["homingBeams"].arrayValue)
		{
			ChartHomingBeam beam;
			beam.chargeTime = b["chargeTime"].AsFloat(0.0f);
			beam.lockTime = b["lockTime"].AsFloat(0.0f);
			beam.hitTime = b["hitTime"].AsFloat(0.0f);
			InitBeamScatter(beam); // 探索中に表示するバラけた線の初期位置を決めておく
			m_HomingBeams.push_back(beam);
		}
		// ロック時刻の昇順にソート
		std::sort(m_HomingBeams.begin(), m_HomingBeams.end(),
			[](const ChartHomingBeam& a, const ChartHomingBeam& b) { return a.lockTime < b.lockTime; });
	}

	// ── 再生開始状態にリセット ──
	m_Started = false;
	m_AudioStarted = false;
	m_MusicTime = 0.0f;
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;

	OutputDebugStringA(("[ChartPlayObject] Loaded: charts/" + name + ".json notes=" +
		std::to_string(m_Notes.size()) + " beams=" + std::to_string(m_HomingBeams.size()) + "\n").c_str());
	return true;
}

void ChartObject::DrawLaneGuideLines(Camera* cam)
{
	if (!m_BeamModelLoaded) return; // 線描画にはビームモデルの仕組みを流用しているため必須

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

		DrawBeamSegmentWithColor(cam, from, to, 3.0f, 3.0f, color, 0.1f);
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

void ChartObject::TriggerScreenShake(float duration, float strength)
{
	//既に揺れている場合長い方を優先する
	if (duration > m_ShakeDuration - m_ShakeTimer)
	{
		m_ShakeDuration = duration;
		m_ShakeTimer = duration;
	}
	m_ShakeStrength = std::max(m_ShakeStrength, strength);
}

void ChartObject::TriggerFlash(float duration)
{
	m_FlashDuration = duration;
	m_FlashTimer = duration;
}

//オート用　到達時刻を過ぎたらパーフェクトに確定させる
void ChartObject::UpdateAutoJudgge()
{
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;

		if (note.IsTap())
		{
			if (m_MusicTime >= note.time)
			{
				m_Perfect++;
				sound.Play(SOUND_METAL_1);
				OnJudge(note.lane, 0);
				note.judged = true;
			}
		}
		else if (note.IsFlick())
		{
			if (m_MusicTime >= note.time)
			{
				int laneFrom = note.lane;
				int laneTo = (note.flickLaneTo >= 0) ? note.flickLaneTo : note.lane;
				int lo = std::min(laneFrom, laneTo);
				int hi = std::max(laneFrom, laneTo);

				m_Perfect++;
				for (int L = lo; L <= hi; L++)
				{
					if (L < 0 || L >= 6) continue;
					OnJudge(L, 0);
				}
				note.judged = true;
			}
		}
		else if (note.IsLong())
		{
			// 先端到達で自動的に押し始めが成立
			if (!note.longStarted && m_MusicTime >= note.arrivalTime)
			{
				m_Perfect++;
				sound.Play(SOUND_SUZU);
				OnJudge(note.lane, 0);
				note.longStarted = true;
				note.nextTickTime = note.arrivalTime + kLongTickInterval;
			}

			// 押しっぱなし中の継続判定（オートなので常に成功）
			if (note.longStarted)
			{
				while (note.nextTickTime <= m_MusicTime && note.nextTickTime < note.time)
				{
					m_Perfect++;
					sound.Play(SOUND_SUZU);
					note.nextTickTime += kLongTickInterval;
				}
			}

			// 終了時刻に到達したら確定（longFailedには絶対にならない）
			if (m_MusicTime >= note.time)
			{
				if (note.longStarted) m_Perfect++;
				sound.Play(SOUND_LONG_END);
				note.judged = true;
			}
		}
	}
}

// ============================================================
// EvaluateNotePosition : TAP/FLICK用の位置補間。
//   spawnTime〜timeの範囲でtを[0,1]にクランプしてから、
//   制御点の数に応じて直線/2次ベジェ/3次ベジェで補間する。
// ============================================================
Vector3 ChartObject::EvaluateNotePosition(const ChartNote& note, float now) const
{
	float startTime = note.GetSpawnTime();
	float endTime = note.time;
	float duration = endTime - startTime;
	if (duration < 0.0001f) duration = 0.0001f; // ゼロ除算防止

	float t = (now - startTime) / duration;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	const auto& p0 = note.spawnPos;
	const auto& p3 = note.judgePos;

	// 制御点なし → 直線補間
	if (note.controlPoints.empty())
	{
		return LerpVector3(p0, p3, t);
	}
	// 制御点1個 → 2次ベジェ
	if (note.controlPoints.size() == 1)
	{
		const auto& p1 = note.controlPoints[0];
		return LerpVector3(LerpVector3(p0, p1, t), LerpVector3(p1, p3, t), t);
	}
	// 制御点2個 → 3次ベジェ（De Casteljauアルゴリズム）
	const auto& p1 = note.controlPoints[0];
	const auto& p2 = note.controlPoints[1];
	auto a = LerpVector3(p0, p1, t);
	auto b = LerpVector3(p1, p2, t);
	auto c = LerpVector3(p2, p3, t);
	return LerpVector3(LerpVector3(a, b, t), LerpVector3(b, c, t), t);
}

// ============================================================
// EvaluateLongNotePosition : LONGNOTE専用の位置補間。
//   spawnTime〜arrivalTimeの速度を基準に、tを「クランプせず」直線補間する。
//   これにより、先端(sentan)がarrivalTimeを過ぎても同じ速度で
//   判定ラインの奥へ進み続けられる（＝押している間、鎖が伸び続けるように見える）。
// ============================================================
Vector3 ChartObject::EvaluateLongNotePosition(const ChartNote& note, float now) const
{
	float startTime = note.GetSpawnTime();
	float arriveTime = note.arrivalTime;
	float duration = arriveTime - startTime;
	if (duration < 0.0001f) duration = 0.0001f;

	float t = (now - startTime) / duration; // クランプしない
	return LerpVector3(note.spawnPos, note.judgePos, t);
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
	m_BeamShaderTime += deltaTime;

	//画面シェイク
	if (m_ShakeTimer > 0.0f)
	{
		m_ShakeTimer -= deltaTime;
		if (m_ShakeTimer < 0.0f) m_ShakeTimer = 0.0f;

		//残り時間の割合で強度を減衰させる(終盤ほど揺れが小さくなる)
		float t = (m_ShakeDuration > 0.0001f) ? (m_ShakeTimer / m_ShakeDuration) : 0.0f;
		float currentStrength = m_ShakeStrength * t;

		float ox = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
		float oy = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentStrength;
		m_ShakeOffset = Vector3(ox, oy, 0.0f);

		if (m_ShakeTimer <= 0.0f)
		{
			m_ShakeStrength = 0.0f;
			m_ShakeOffset = Vector3::Zero;
		}
	}
	else
	{
		m_ShakeOffset = Vector3::Zero;
	}

	//フラッシュの更新
	if (m_FlashTimer > 0.0f)
	{
		m_FlashTimer -= deltaTime;
		if (m_FlashTimer < 0.0f) m_FlashTimer = 0.0f;
	}

	// コンボ数字のポップアニメーションのタイマー減算
	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

	//判定表示時間の減算
	if (m_JudgeRankPopTimer > 0.0f)
	{
		m_JudgeRankPopTimer -= deltaTime;
		if (m_JudgeRankPopTimer < 0.0f) m_JudgeRankPopTimer = 0.0f;
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

	//ボタン入力時に出る障壁
	for (int i = 0; i < 6; i++)
	{
		if (!m_BarrierEffects[i].active) continue;
		m_BarrierEffects[i].timer += deltaTime;
		if (m_BarrierEffects[i].timer >= kBarrierDuration)
			m_BarrierEffects[i].active = false;
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
	// LONG/FLICKは専用の判定関数(JudgeLongNoteHold/JudgeFlickNotes)側でミス処理する
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (!note.IsTap()) continue;
		if (m_MusicTime - note.time > kGud)
		{
			note.judged = true;
			m_Miss++;
		}
	}

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
		UpdateAutoJudgge();
	}
	else
	{
		// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
		// LONG/FLICKは専用の判定関数(JudgeLongNoteHold/JudgeFlickNotes)側でミス処理する
		for (auto& note : m_Notes)
		{
			if (note.judged) continue;
			if (!note.IsTap()) continue;
			if (m_MusicTime - note.time > kGud)
			{
				note.judged = true;
				m_Miss++;
			}
		}

		// S/D/F/J/K/Lキーで、TAPノーツのトリガー判定を行う
		for (int lane = 0; lane < 6; lane++)
		{
			JudgeLane(kLaneKeys[lane], lane);
		}

		// フリック・ロングノーツの判定
		JudgeFlickNotes();
		JudgeLongNoteHold();
	}

	// 判定済み(judged=true)になったノーツをまとめて削除
	// （TAPの判定はJudgeLaneで即座にjudged=trueにするが、実削除はここでまとめて行う）
	m_Notes.erase(
		std::remove_if(m_Notes.begin(), m_Notes.end(),
			[](const ChartNote& n) { return n.judged; }),
		m_Notes.end());

	// ホーミングビームの状態遷移とプレイヤー移動の更新
	UpdateHomingBeams(deltaTime);
}

// ============================================================
// JudgeLane : 指定キーを「押した瞬間」に、そのレーンで
//             最も判定時刻が近いTAPノーツを判定する。
// ============================================================
void ChartObject::JudgeLane(int key, int laneIndex)
{
	if (!Input::GetKeyTrigger(key)) return; // 押した瞬間でなければ何もしない

	int bestIndex = -1;
	float bestDiff = 9999.0f;

	// 対象レーンのTAPノーツの中から、最も時間差が小さいものを探す
	for (int i = 0; i < (int)m_Notes.size(); i++)
	{
		if (m_Notes[i].judged) continue;
		if (m_Notes[i].lane != laneIndex) continue;
		if (!m_Notes[i].IsTap()) continue; // LONG/FLICKはここでは判定しない

		float diff = std::fabs(m_Notes[i].time - m_MusicTime);
		if (diff < bestDiff) { bestDiff = diff; bestIndex = i; }
	}
	if (bestIndex == -1) return;
	if (bestDiff > kGud) return; // 判定圏外なら何もしない

	// 時間差に応じてランクを決定
	int rank = (bestDiff < kPerfect) ? 0 : (bestDiff < kGreat) ? 1 : 2;
	if (rank == 0)
	{
		m_Perfect++;
		sound.Play(SOUND_METAL_1);
	}
	else if (rank == 1) 
	{
		m_Great++; 
		sound.Play(SOUND_METAL_1);
	}
	else if (rank == 2) 
	{
		m_Good++; 
		sound.Play(SOUND_METAL_1);
	}

	m_Notes[bestIndex].judged = true;
	OnJudge(laneIndex, rank);
}

// ============================================================
// JudgeFlickNotes : 横フリックの判定（時間ベース）。
//   毎フレーム呼び出し、判定ウィンドウ(±kGud)に入っている間、
//   プレイヤーが要求サイドにいれば時間差でランク判定して成立させる。
//   ウィンドウを過ぎてしまったらミス確定。
// ============================================================
void ChartObject::JudgeFlickNotes()
{
	for (auto& n : m_Notes)
	{
		if (n.judged) continue;
		if (!n.IsFlick()) continue;

		float diff = n.time - m_MusicTime;    // 正：まだ来ていない　負：過ぎた
		int requiredSide = n.IsFlickRight() ? 1 : -1; // 右フリックなら1、左フリックなら-1
		int currentSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;

		if (fabsf(diff) <= kGud && currentSide == requiredSide)
		{
			float absDiff = fabsf(diff);
			int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;

			// レーン範囲（From〜To）を求めて、範囲内すべてのレーンに判定エフェクトを出す
			int laneFrom = n.lane;
			int laneTo = (n.flickLaneTo >= 0) ? n.flickLaneTo : n.lane;
			int lo = std::min(laneFrom, laneTo);
			int hi = std::max(laneFrom, laneTo);

			if (rank == 0) m_Perfect++;
			else if (rank == 1) m_Great++;
			else m_Good++;

			for (int L = lo; L <= hi; L++)
			{
				if (L < 0 || L >= 6) continue;
				OnJudge(L, rank);
			}

			n.judged = true;
			continue;
		}

		// 判定ウィンドウを過ぎても成立しなかった場合はミス確定
		if (diff < -kGud)
		{
			n.judged = true;
			m_Miss++;
		}
	}
}

// ============================================================
// JudgeLongNoteHold : ロングノーツの押しっぱなし判定。
//   1. 先端が判定ラインに到達するタイミングでキーが押されていれば「開始」成立
//   2. 開始後は一定間隔(kLongTickInterval)ごとにtick加算（継続判定）
//   3. 終了時刻まで押しっぱなしならクリア、途中で離したら即ミス
// ============================================================
void ChartObject::JudgeLongNoteHold()
{
	for (auto& n : m_Notes)
	{
		if (!n.IsLong()) continue;
		if (n.judged) continue;

		if (n.lane < 0 || n.lane >= 6) continue;
		bool isDown = Input::GetKeyPress(kLaneKeys[n.lane]);

		// ── まだ開始していない場合 ──
		if (!n.longStarted && !n.longFailed)
		{
			float diff = n.arrivalTime - m_MusicTime;
			if (isDown && fabsf(diff) <= kGud)
			{
				float absDiff = fabsf(diff);
				int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;
				if (rank == 0) m_Perfect++;
				else if (rank == 1) m_Great++;
				else m_Good++;
				OnJudge(n.lane, rank);

				n.longStarted = true;
				n.nextTickTime = n.arrivalTime + kLongTickInterval;
			}
			else if (diff < -kGud)
			{
				// 開始を逃した → Failed扱いにするだけ。ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}
		// ── 開始済み・まだ成功継続中 ──
		else if (n.longStarted && !n.longFailed)
		{
			if (isDown)
			{
				while (n.nextTickTime <= m_MusicTime && n.nextTickTime < n.time)
				{
					m_Perfect++;
					n.nextTickTime += kLongTickInterval;
				}
			}
			else
			{
				// 離した瞬間 → Failedにするが、ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}

		// ── 終了時刻に到達したら、ここで初めて judged=true にする ──
		if (m_MusicTime >= n.time)
		{
			if (!n.longFailed) m_Perfect++; // 最後まで成功していればクリア扱い
			n.judged = true;
		}
	}
}

// ============================================================
// InitBeamScatter : ビームの「収束前バラけ位置」をランダムに決める。
//   プレイヤー予測位置の周囲、上下左右にランダムなオフセットを複数用意し、
//   探索中(Homing状態)にこの位置から線が伸びるように見せる。
// ============================================================
void ChartObject::InitBeamScatter(ChartHomingBeam& beam)
{
	for (int i = 0; i < ChartHomingBeam::kScatterCount; i++)
	{
		float angle = ((float)rand() / RAND_MAX) * DirectX::XM_2PI;
		float radius = 150.0f + ((float)rand() / RAND_MAX) * 250.0f;    // 150〜400の範囲でばらける
		float heightOffset = ((float)rand() / RAND_MAX - 0.5f) * 200.0f; // 高さ方向のばらつき

		beam.scatterOffsets[i] = Vector3(
			cosf(angle) * radius,
			heightOffset,
			sinf(angle) * radius
		);
	}
}

// ============================================================
// UpdateHomingBeams : 毎フレーム呼ばれる、ビーム全体の更新処理。
//   1. プレイヤーの左右移動の補間更新
//   2. 各ビームの状態遷移：
//        Homing（探索中。タンタンタン→ロック確定）
//        → Fired（発射済み。一定間隔で命中/回避判定）
//        → Resolved（判定終了）
// ============================================================
void ChartObject::UpdateHomingBeams(float deltaTime)
{
	// ── プレイヤー側の状態はPlayerクラスから取得（入力判定・移動補間はPlayer::Updateで実施済み）──
	int playerSide = m_PlayerRef ? m_PlayerRef->GetSide() : 0;

	// ── 各ビームの状態遷移 ──
	for (auto& beam : m_HomingBeams)
	{
		if (beam.state == ChartHomingBeam::State::Resolved) continue;
		if (m_MusicTime < beam.chargeTime) continue;

		{
			float total = beam.lockTime - beam.chargeTime;
			if (total < 0.01f) total = 0.01f;
			float beatInterval = total / ChartHomingBeam::kSearchBeatCount;
			while (beam.searchBeatsPlayed < ChartHomingBeam::kSearchBeatCount)
			{
				float beatTime = beam.chargeTime + beatInterval * beam.searchBeatsPlayed;
				if (m_MusicTime >= beatTime)
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
				beam.lockedPos = GetPlayerSideAnchor(playerSide);
				beam.lockedPos.x += playerSide * 150.0f; // 旧m_PlayerMoveRange相当。必要ならPlayer::GetMoveRange()を公開して参照
			}

			if (m_MusicTime >= beam.lockTime)
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
			if (m_MusicTime >= beam.hitTime)
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
					while (beam.nextCountTime < beam.hitTime && m_MusicTime >= beam.nextCountTime)
					{
						bool dodged = (playerSide != beam.lockedSide);
						if (dodged)
						{
							//m_Perfect++;
							TriggerScreenShake(0.1f, 4.0f);
						}
						else
						{
							//m_Miss++;
							TriggerScreenShake(0.25f, 15.0f);
							TriggerFlash(0.15f);
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
// OnJudge : 判定がヒットした瞬間に呼ばれるフック。
//   コンボを加算し、判定円の位置に花びらパーティクルを100個生成する。
// ============================================================
void ChartObject::OnJudge(int laneIndex, int rank)
{
	m_Combo++;

	if (laneIndex >= 0 && laneIndex < 6)
	{
		m_BarrierEffects[laneIndex].active = true;
		m_BarrierEffects[laneIndex].timer = 0.0f;
	}

	if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;
	m_ComboPopTimer = 0.15f; // コンボ数字が一瞬拡大表示される演出用タイマー

	m_JudgeRankCurrent = rank;              // 0=Perfect, 1=Great, 2=Good
	m_JudgeRankPopTimer = kJudgeRankPopDuration;

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
	(void)rank; // 現状rank別の演出差はまだ未実装。将来ランクごとに色や量を変える場合はここで分岐する
}

// ============================================================
// DrawBeamSegment : from→to の間にビームモデルを引き伸ばして1本描画する。
//   モデルのローカルZ軸方向をdir(from→to方向)に向け、
//   X/Yスケールで太さを、Zスケールで長さを調整する。
// ============================================================
void ChartObject::DrawBeamSegment(Camera* cam,
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
			DrawModel(m_BeamMeshRenderer, m_BeamShader, m_BeamSubsets, m_BeamMaterials, m_BeamTextures, world);
		};
	// 外側から描くと、内側の明るいコアが最後に重なって一番目立つ
	drawLayer(0.3f);   // グロー：太く薄く広がる外周
	drawLayer(0.6f);   // ミッド：中間の明るさ
	drawLayer(1.0f);   // コア：細く一番明るい芯

	Renderer::SetBlendState(BS_NONE);

	//Matrix world =
	//	Matrix::CreateScale(thicknessX, thicknessY, len / m_BeamModelLength) *
	//	basis *
	//	Matrix::CreateTranslation(mid);

	//DrawModel(m_BeamMeshRenderer, m_BeamShader, m_BeamSubsets, m_BeamMaterials, m_BeamTextures, world);
}

void ChartObject::DrawBeamSegmentWithColor(Camera* cam, const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to, float thicknessX, float thicknessY, const DirectX::SimpleMath::Vector3& baseColor, float intensity)
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
		Matrix::CreateTranslation(mid);

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

void ChartObject::DrawJudggedBeam(Camera* cam, const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to, float thickness, const DirectX::SimpleMath::Vector3& color, float intensity, int segments, float jitter)
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
// DrawHomingBeams : 全ビームの描画をまとめて行う。
//   Homing中：探索中の「バラけた線」が収束していく様子を表現
//   Fired中　：ロック位置へ向けて1本の太いビームを描画
// ============================================================
void ChartObject::DrawHomingBeams(Camera* cam)
{
	if (!m_BeamModelLoaded) return;
	if (m_HomingBeams.empty()) return;

	// ビームの発射元座標。Enemyの参照が無ければ仮の固定値を使う
	Vector3 beamOrigin = m_EnemyRef
		? (m_EnemyRef->GetPosition() + Vector3(0.0f, 50.0f, 0.0f))
		: Vector3(0.0f, 0.0f, 500.0f);

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_ALPHABLEND);
	//Renderer::SetBlendState(BS_NONE);

	for (auto& beam : m_HomingBeams)
	{
		if (beam.state == ChartHomingBeam::State::Resolved) continue;
		if (m_MusicTime < beam.chargeTime) continue;

		// ロック確定済みならロック座標へ、まだならプレイヤーの現在位置へ収束させる
		Vector3 lockTarget = beam.judgmentLocked
			? beam.lockedPos
			: GetPlayerSideAnchor(m_PlayerRef ? m_PlayerRef->GetSide() : 0);

		Vector3 color = (beam.state == ChartHomingBeam::State::Fired)
			? Vector3(1.0f, 0.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);

		if (beam.state == ChartHomingBeam::State::Homing)
		{
			// 収束の進行度t(0=バラバラ, 1=収束完了)をease-inで計算
			float total = beam.lockTime - beam.chargeTime;
			if (total < 0.01f) total = 0.01f;
			float t = std::clamp((m_MusicTime - beam.chargeTime) / total, 0.0f, 1.0f);
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
		Vector3 headPos = EvaluateLongNotePosition(note, headTime);
		{
			Matrix world = Matrix::CreateScale(20.0f) * Matrix::CreateTranslation(headPos);
			DrawModel(sentanRenderer, sentanShader, sentanSubsets, sentanMaterials, sentanTextures, world);
		}

		// ── 3) o（終端の輪）の現在位置 ──
		// sentanよりholdDuration秒だけ遅れて追従する（まだ出現範囲に入っていなければ非表示）
		float tailTime = headTime - holdDuration;
		bool tailVisible = (tailTime >= displayStart);
		if (tailVisible)
		{
			Vector3 tailPos = EvaluateLongNotePosition(note, tailTime);
			Matrix world = Matrix::CreateScale(20.0f) * Matrix::CreateTranslation(tailPos);
			DrawModel(oRenderer, oShader, oSubsets, oMaterials, oTextures, world);
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
					Vector3 pos = EvaluateLongNotePosition(note, sampleTime);
					Matrix world = Matrix::CreateScale(10.0f) * Matrix::CreateTranslation(pos);
					DrawModel(douRenderer, douShader, douSubsets, douMaterials, douTextures, world);
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

		Vector3 pos = EvaluateNotePosition(note, m_MusicTime);

		// 右フリックと左フリックで回転を反転（※モデルの向きに応じて調整が必要）
		float rotY = note.IsFlickRight() ? 0.0f : DirectX::XM_PI;

		Matrix world =
			Matrix::CreateScale(m_NoteScale * 0.7f) *
			Matrix::CreateRotationY(rotY) *
			Matrix::CreateTranslation(pos);

		DrawModel(renderer, shader, subsets, materials, textures, world);
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
	cam->SetShakeOffset(m_ShakeOffset);
	// 3Dカメラを再アサート（他オブジェクトがTexture2Dで2Dに切り替えている場合があるため）
	cam->SetCamera(0);

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

		Vector3 pos = EvaluateNotePosition(note, m_MusicTime);
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
			? EvaluateLongNotePosition(note, m_MusicTime)
			: EvaluateNotePosition(note, m_MusicTime);

		float ringScale = 100.0f + t * 60.0f;
		float ringAlpha = 0.2f + t * 0.6f;
		m_ApproachRing.Draw(pos, ringScale, ringAlpha);
	}
	Renderer::SetBlendState(BS_NONE);

	// ── レーンごとに一番近い未判定ノーツの接近度(0〜1)を計算 ──
	// （判定円ガイドの「育つ四角」の大きさに使う。フリックは対象外）
	float laneT[6] = { 0,0,0,0,0,0 };
	bool laneActive[6] = { false,false,false,false,false,false };
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
		}
	}

	// ── 判定円アイコン：judgeCirclesの3D座標を画面に投影して表示 ──
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
			constexpr float kFillMinSize = 10.0f;
			float fillSize = kFillMinSize + laneT[i] * (m_FrameSize - kFillMinSize);
			m_LaneFill[i].SetScale(fillSize, fillSize, 1.0f);
			m_LaneFill[i].SetPosition(screenPos);
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

	// 判定ランク演出
	if (m_JudgeRankPopTimer > 0.0f && m_JudgeRankCurrent >= 0)
	{
		// フェードアウト：残り時間の割合をアルファ的な演出に使ってもいい（今回はスケールで演出）
		float t = m_JudgeRankPopTimer / kJudgeRankPopDuration; // 1.0→0.0
		float popScale = 1.0f + (1.0f - t) * 0.0f;

		Texture2D* target = nullptr;
		switch (m_JudgeRankCurrent)
		{
		case 0: target = &m_JudgeRankPerfect; break;
		case 1: target = &m_JudgeRankGreat;   break;
		case 2: target = &m_JudgeRankGood;    break;
		}

		if (target)
		{
			Renderer::SetBlendState(BS_ALPHABLEND);
			target->SetPosition(0.0f, 30.0f, 0.0f);
			target->Draw(cam);
			Renderer::SetBlendState(BS_NONE);
		}
	}

	// ── コンボ数表示（3桁、ヒット直後は少し拡大する演出）──
	{
		int combo = m_Combo;
		if (combo > 999) combo = 999;

		int digits[3] = { combo / 100,(combo / 10) % 10, combo % 10 };
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 40.0f;
		float startX = 200.0f;

		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 3; i++)
		{
			m_ComboDigits[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
			m_ComboDigits[i].SetScale(m_NuberSplite_Size * popScale, m_NuberSplite_Size * popScale, 1.0f);
			m_ComboDigits[i].SetPosition(startX + digitW * i, 250.0f, 0.0f);
			m_ComboDigits[i].Draw(cam);
		}
	}

	// ── 判定内訳の表示（PERFECT/GREAT/GOOD/MISS）──
	{
		auto drawCountRow = [&](Texture2D* row, int value, float y)
			{
				if (value > 9999) value = 9999;
				int digits[4] = { value / 1000, value / 100, (value / 10) % 10, value % 10 };
				float digitW = 26.0f;   // コンボより詰めて表示
				float startX = 40.0f;

				for (int i = 0; i < 4; i++)
				{
					row[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
					row[i].SetPosition(startX + digitW * i, y - 100.0f, 0.0f);
					row[i].Draw(cam);
				}
			};
		float topY = 400.0f;	//一番上のY座標
		float rowSpacing = 80.0f;	//行間

		Renderer::SetBlendState(BS_ALPHABLEND);
		drawCountRow(m_PerfectDigits, m_Perfect, topY);
		drawCountRow(m_GreatDigits, m_Great, topY - rowSpacing);
		drawCountRow(m_GoodDigits, m_Good, topY - rowSpacing * 2);
		drawCountRow(m_MissDigits, m_Miss, topY - rowSpacing * 3);
		Renderer::SetBlendState(BS_NONE);
	}

	Renderer::SetBlendState(BS_ADDITIVE);
	for (int i = 0; i < 6; i++)
	{
		if (!m_BarrierEffects[i].active) continue;
		float progress = m_BarrierEffects[i].timer / kBarrierDuration;
		Vector3 pos = m_JudgeCircles[i].GetPosition(m_MusicTime);
		m_BarrierQuad.Draw(pos, 3000.0f, progress, GetLaneColor(i));
	}
	Renderer::SetBlendState(BS_NONE);

	// ── ホーミングビームの描画（他要素の最後に描くことで前面に出す）──
	DrawHomingBeams(cam);

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