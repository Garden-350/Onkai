#pragma once
#include "ChartHomingBeam.h"
#include "ScreenEffects.h"
#include "StaticMesh.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Texture.h"
#include "Shader.h"
#include <SimpleMath.h>
#include <vector>
#include <memory>

class Camera;
class Sound;
class Enemy;

// ============================================================
// HomingBeamSystem : ホーミングビームの状態(m_HomingBeams)とビーム用モデル一式を
//   保持し、更新・描画をまとめて行うクラス。
//   ChartObjectが持っていたビーム関連のメンバと処理を丸ごと移設したもの。
//   ロジックは元のChartObject::UpdateHomingBeams / DrawHomingBeams等と同一。
// ============================================================
class HomingBeamSystem
{
public:
	// ビーム用モデルのロード（ChartObject::Init から呼ぶ）
	void Init();

	// LoadChart後、読み込んだビーム一覧をセットする
	void SetBeams(std::vector<ChartHomingBeam> beams) { m_HomingBeams = std::move(beams); }
	size_t GetBeamCount() const { return m_HomingBeams.size(); }

	// 毎フレーム呼ぶ：状態遷移の更新（探索→ロック→発射→解決）
	//   musicTime        : 現在の楽曲内時刻
	//   playerSide       : プレイヤーの左右入力サイド(-1/0/1)
	//   playerSideAnchor : ChartObject::GetPlayerSideAnchor(playerSide) の結果
	//                      （judgeCirclesに依存するためChartObject側で計算して渡す）
	void Update(float deltaTime, float musicTime, int playerSide,
		const DirectX::SimpleMath::Vector3& playerSideAnchor,
		Sound& sound, ScreenEffects& effects);

	// 全ビームの描画（他要素の最後に描くことを想定）
	void Draw(Camera* cam, float musicTime, Enemy* enemyRef,
		const DirectX::SimpleMath::Vector3& playerSideAnchor);

	// DrawLightEffects（ChartObject側に残置）と共有するビーム描画プリミティブ
	void DrawBeamSegmentWithColor(Camera* cam, const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to,
		float thicknessX, float thicknessY, const DirectX::SimpleMath::Vector3& baseColor, float intensity);

	bool IsModelLoaded() const { return m_BeamModelLoaded; }

private:
	void DrawBeamSegment(Camera* cam, const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to,
		float thicknessX, float thicknessY);
	void DrawJudggedBeam(Camera* cam, const DirectX::SimpleMath::Vector3& from, const DirectX::SimpleMath::Vector3& to,
		float thickness, const DirectX::SimpleMath::Vector3& color, float intensity,
		int segments = 6, float jitter = 15.0f);

	std::vector<ChartHomingBeam> m_HomingBeams;

	// ────────────────────────────────────────
	// ホーミングビーム用モデル（棒状FBXを引き伸ばして使う想定）
	// ────────────────────────────────────────
	bool m_BeamModelLoaded = false;
	MeshRenderer m_BeamMeshRenderer;
	Texture m_BeamNoiseTexture;
	Shader m_BeamShader;
	std::vector<SUBSET> m_BeamSubsets;
	std::vector<std::unique_ptr<Material>> m_BeamMaterials;
	std::vector<std::unique_ptr<Texture>> m_BeamTextures;
	float m_BeamModelLength = 1.0f;	// モデルのローカルZ方向の長さ（アセットの実寸に合わせて調整）
	float m_BeamShaderTime = 0.0f;
};
