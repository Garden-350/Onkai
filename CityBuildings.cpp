#include "CityBuildings.h"
#include "Renderer.h"
#include <cstdlib>
#include <cassert>

using namespace DirectX::SimpleMath;

namespace
{
	constexpr int   kBuildingCount = 260;			// ビルの本数
	constexpr float kGroundY = -300.0f;			// Groundの表面の高さに合わせる
	constexpr float kSpawnHalfExtent = 1800.0f;	// この範囲(X,Z)にランダム配置する

	// この矩形の中にはビルを生成しない（プレイエリア/ノーツ/判定円を隠さないための除外ゾーン）
	constexpr float kExclusionMinX = -400.0f;
	constexpr float kExclusionMaxX = 300.0f;
	constexpr float kExclusionMinZ = -450.0f;
	constexpr float kExclusionMaxZ = 550.0f;

	constexpr float kMinWidth = 40.0f, kMaxWidth = 90.0f;
	constexpr float kMinDepth = 40.0f, kMaxDepth = 90.0f;
	constexpr float kMinHeight = 80.0f, kMaxHeight = 450.0f;

	constexpr float kWindowTileSize = 50.0f;	// 窓の大きさが揃って見えるようにするテクスチャの繰り返し単位(ワールド単位)

	float RandRange(float lo, float hi)
	{
		return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
	}

	bool InExclusionZone(float x, float z)
	{
		return x >= kExclusionMinX && x <= kExclusionMaxX &&
			z >= kExclusionMinZ && z <= kExclusionMaxZ;
	}
}

void CityBuildings::Init()
{
	m_Vertices.clear();
	m_Indices.clear();

	for (int i = 0; i < kBuildingCount; i++)
	{
		float x, z;
		//除外ゾーンに入らない座標が出るまでやり直す
		do
		{
			x = RandRange(-kSpawnHalfExtent, kSpawnHalfExtent);
			z = RandRange(-kSpawnHalfExtent, kSpawnHalfExtent);
		} while (InExclusionZone(x, z));

		float width = RandRange(kMinWidth, kMaxWidth);
		float depth = RandRange(kMinDepth, kMaxDepth);
		float height = RandRange(kMinHeight, kMaxHeight);

		//ビルごとに少しずつ違う色味（白系テクスチャに掛け合わせる色）
		float baseGray = RandRange(0.55f, 0.85f);
		Color tint(
			baseGray + RandRange(-0.05f, 0.05f),
			baseGray + RandRange(-0.05f, 0.05f),
			baseGray + RandRange(0.0f, 0.1f),	//ほんの少し青み寄りにする
			1.0f
		);

		AddBuilding(x, z, width, depth, height, tint);
	}

	m_VertexBuffer.Create(m_Vertices);
	m_IndexBuffer.Create(m_Indices);

	m_Shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");

	bool sts = m_Texture.Load("assets/texture/building.png");
	assert(sts == true);

	m_Material = std::make_unique<Material>();
	MATERIAL mtrl{};
	mtrl.Diffuse = Color(1, 1, 1, 1);
	mtrl.TextureEnable = true;
	m_Material->Create(mtrl);
}

// centerX/centerZ: ビル底面の中心座標　width/depth: 底面のサイズ　height: 高さ
void CityBuildings::AddBuilding(float centerX, float centerZ, float width, float depth, float height,
	const Color& tint)
{
	float x0 = centerX - width * 0.5f;
	float x1 = centerX + width * 0.5f;
	float z0 = centerZ - depth * 0.5f;
	float z1 = centerZ + depth * 0.5f;
	float yBase = kGroundY;
	float yTop = kGroundY + height;

	float uWidth = width / kWindowTileSize;
	float uDepth = depth / kWindowTileSize;
	float vHeight = height / kWindowTileSize;

	auto addQuad = [&](Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3,
		Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3)
		{
			unsigned int baseIndex = (unsigned int)m_Vertices.size();

			//Groundと同じく、法線は実際の頂点配置から外積で自動計算する
			Vector3 e1 = p1 - p0;
			Vector3 e2 = p2 - p0;
			Vector3 normal = e1.Cross(e2);
			normal.Normalize();

			VERTEX_3D v0{}, v1{}, v2{}, v3{};
			v0.position = p0; v1.position = p1; v2.position = p2; v3.position = p3;
			v0.uv = uv0; v1.uv = uv1; v2.uv = uv2; v3.uv = uv3;
			v0.normal = v1.normal = v2.normal = v3.normal = normal;
			v0.color = v1.color = v2.color = v3.color = tint;

			m_Vertices.push_back(v0);
			m_Vertices.push_back(v1);
			m_Vertices.push_back(v2);
			m_Vertices.push_back(v3);

			//三角形2枚（Groundと同じ並び：0-1-2, 2-1-3）
			m_Indices.push_back(baseIndex + 0);
			m_Indices.push_back(baseIndex + 1);
			m_Indices.push_back(baseIndex + 2);
			m_Indices.push_back(baseIndex + 2);
			m_Indices.push_back(baseIndex + 1);
			m_Indices.push_back(baseIndex + 3);
		};

	// +Z面
	addQuad(
		Vector3(x0, yBase, z1), Vector3(x1, yBase, z1),
		Vector3(x0, yTop, z1), Vector3(x1, yTop, z1),
		Vector2(0, vHeight), Vector2(uWidth, vHeight), Vector2(0, 0), Vector2(uWidth, 0));

	// -Z面
	addQuad(
		Vector3(x1, yBase, z0), Vector3(x0, yBase, z0),
		Vector3(x1, yTop, z0), Vector3(x0, yTop, z0),
		Vector2(0, vHeight), Vector2(uWidth, vHeight), Vector2(0, 0), Vector2(uWidth, 0));

	// +X面
	addQuad(
		Vector3(x1, yBase, z1), Vector3(x1, yBase, z0),
		Vector3(x1, yTop, z1), Vector3(x1, yTop, z0),
		Vector2(0, vHeight), Vector2(uDepth, vHeight), Vector2(0, 0), Vector2(uDepth, 0));

	// -X面
	addQuad(
		Vector3(x0, yBase, z0), Vector3(x0, yBase, z1),
		Vector3(x0, yTop, z0), Vector3(x0, yTop, z1),
		Vector2(0, vHeight), Vector2(uDepth, vHeight), Vector2(0, 0), Vector2(uDepth, 0));

	// 屋上（俯瞰されることもあるので一応張っておく）
	addQuad(
		Vector3(x0, yTop, z1), Vector3(x1, yTop, z1),
		Vector3(x0, yTop, z0), Vector3(x1, yTop, z0),
		Vector2(0, 0), Vector2(uWidth, 0), Vector2(0, uDepth), Vector2(uWidth, uDepth));
}

void CityBuildings::Draw(Camera* cam)
{
	cam->SetCamera();
	Renderer::SetCullMode(CULL_BACK);
	Renderer::SetDepthEnable(true);

	Matrix worldmtx = Matrix::Identity;	//座標は生成時にワールド座標で直接計算済み
	Renderer::SetWorldMatrix(&worldmtx);

	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	m_Texture.SetGPU();
	m_Material->SetGPU();

	devicecontext->DrawIndexed((UINT)m_Indices.size(), 0, 0);
}