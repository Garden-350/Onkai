#include "ChartLoader.h"
#include "SimpleJson.h"
#include <algorithm>
#include <cmath>
#undef min
#undef max

using namespace DirectX::SimpleMath;

namespace
{
	// ChartObject.cpp の無名namespaceから移設（judgeCirclesのX座標変換にのみ使う定数）
	constexpr float kXScale = 1.7f;	//540にこの値を掛ける
	constexpr float kEditorRawCenterX = -85.0f;

	// ============================================================
	// InitBeamScatter : ビームの「収束前バラけ位置」をランダムに決める。
	//   プレイヤー予測位置の周囲、上下左右にランダムなオフセットを複数用意し、
	//   探索中(Homing状態)にこの位置から線が伸びるように見せる。
	//   （ChartObjectのメンバに依存しない純粋関数なので、そのままローカル関数として移設）
	// ============================================================
	void InitBeamScatter(ChartHomingBeam& beam)
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
}

namespace ChartLoader
{
	// ============================================================
	// Load : charts/<name>.json を読み込んでパースする。
	//   ChartObject::LoadChart から丸ごと移設したロジック。
	//   audioFile の実際の再生準備（sound.LoadDynamic）は
	//   Soundメンバを持たないここでは行わず、呼び出し側(ChartObject)に委ねる
	//   （result.audioFile に読み込むべきファイル名だけを詰めて返す）。
	// ============================================================
	ChartLoadResult Load(const std::string& name, const std::vector<ChartJudgeCircle>& defaultJudgeCircles)
	{
		ChartLoadResult result;
		result.judgeCircles = defaultJudgeCircles;

		JsonValue root;
		std::string err;
		if (!SimpleJson::ParseFile("charts/" + name + ".json", root, &err))
		{
			OutputDebugStringA(("[ChartLoader] Load failed: " + err + "\n").c_str());
			result.success = false;
			return result;
		}

		// ── メタ情報（bpm/beatsPerBarはトップ直下 or "meta"の下のどちらにも対応）──
		if (root.Has("bpm")) result.bpm = root["bpm"].AsFloat(result.bpm);
		else if (root.Has("meta") && root["meta"].Has("bpm")) result.bpm = root["meta"]["bpm"].AsFloat(result.bpm);

		if (root.Has("beatsPerBar")) result.beatsPerBar = root["beatsPerBar"].AsInt(result.beatsPerBar);
		else if (root.Has("meta") && root["meta"].Has("beatsPerBar")) result.beatsPerBar = root["meta"]["beatsPerBar"].AsInt(result.beatsPerBar);

		// ── 音声ファイル名の読み込み ──
		if (root.Has("audioFile"))
			result.audioFile = root["audioFile"].AsString("");
		else if (root.Has("meta") && root["meta"].Has("audioFile"))
			result.audioFile = root["meta"]["audioFile"].AsString("");
		else
			result.audioFile = "";

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

				// フィナーレ演出用：螺旋パラメータ（無ければ0=螺旋なし）
				if (n.Has("spiralTurns"))
					note.spiralTurns = n["spiralTurns"].AsFloat(0.0f);
				if (n.Has("spiralRadius"))
					note.spiralRadius = n["spiralRadius"].AsFloat(0.0f);

				// 状態フラグの初期化
				note.judged = false;
				note.approcachPlayed = false;
				note.longStarted = false;
				note.longFailed = false;

				result.notes.push_back(note);
			}
			// 判定時刻の昇順にソート（描画・判定処理を楽にするため）
			std::sort(result.notes.begin(), result.notes.end(),
				[](const ChartNote& a, const ChartNote& b) {return a.time < b.time; });
		}

		// ── 判定円のキーフレーム読み込み ──
		if (root.Has("judgeCircles") && root["judgeCircles"].IsArray())
		{
			for (const auto& jc : root["judgeCircles"].arrayValue)
			{
				int lane = jc["lane"].AsInt(-1);
				if (lane < 0 || lane >= (int)result.judgeCircles.size()) continue;

				auto& circle = result.judgeCircles[lane];
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
				result.homingBeams.push_back(beam);
			}
			// ロック時刻の昇順にソート
			std::sort(result.homingBeams.begin(), result.homingBeams.end(),
				[](const ChartHomingBeam& a, const ChartHomingBeam& b) { return a.lockTime < b.lockTime; });
		}

		result.success = true;
		return result;
	}
}
