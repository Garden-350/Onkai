#pragma once
#include "Object.h"
#include "StaticMesh.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Texture.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class Oto : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;

	bool Load(const std::string& file, const std::string& texDir);	//AddObject の後で呼ぶ
	bool SetTexture(const std::string& file);
	bool SetMatcap(const std::string& file);

	void SetSpinSpeed(const Vector3& radPerSec) { m_SpinSpeed = radPerSec; }	//自分の真ん中で回る速さ
	void SetOffset(const Vector3& offset) { m_Offset = offset; m_HasOffset = true; }	//ロゴの軸から見た、この部品の真ん中の位置

	static void SetLogoRotation(const Vector3& rad) { s_LogoRotation = rad; }	//ロゴ全体の向き（y が縦の軸の回転）
	static void SetLogoPosition(const Vector3& pos) { s_LogoPosition = pos; }	//軸を置く場所
	static void SetLogoScale(float scale) { s_LogoScale = scale; }
	static void SetLogoAxis(const Vector3& axis) { s_LogoAxis = axis; }			//モデルの座標で見た、回す軸の位置（Blender で原点に合わせたなら 0 のまま）
private:
	MeshRenderer m_MeshRenderer;
	std::vector<SUBSET> m_Subsets;
	std::vector<std::unique_ptr<Texture>> m_Textures;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::unique_ptr<Texture> m_OverrideTexture;
	std::unique_ptr<Texture> m_Matcap;

	Vector3 m_Pivot = Vector3(0.0f, 0.0f, 0.0f);		//この部品の真ん中（頂点を囲む箱の真ん中。自動で測る）
	Vector3 m_Offset = Vector3(0.0f, 0.0f, 0.0f);		//ロゴの中での、この部品の真ん中の位置
	bool m_HasOffset = false;							//SetOffset で決めたか（決めていなければ、書き出した位置のまま）
	Vector3 m_SpinSpeed = Vector3(0.0f, 0.0f, 0.0f);	//自分の真ん中で回る速さ

	bool m_Started = false;
	std::chrono::steady_clock::time_point m_LastUpdateClock;

	inline static Vector3 s_LogoRotation = Vector3(0.0f, 0.0f, 0.0f);
	inline static Vector3 s_LogoPosition = Vector3(0.0f, 0.0f, 0.0f);
	inline static Vector3 s_LogoAxis = Vector3(0.0f, 0.0f, 0.0f);
	inline static float s_LogoScale = 1.0f;
};