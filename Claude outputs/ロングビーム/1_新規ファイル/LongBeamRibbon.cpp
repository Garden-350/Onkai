#include "LongBeamRibbon.h"
#include "Camera.h"
#include <algorithm>
#undef min
#undef max

using namespace DirectX::SimpleMath;

void LongBeamRibbon::Init()
{
	// 最大数ぶんの頂点を先に確保しておき、毎フレーム中身だけ書き換える
	m_Vertices.resize(kMaxPoints * 2);
	for (auto& v : m_Vertices)
	{
		v.position = Vector3::Zero;
		v.normal = Vector3(0.0f, 1.0f, 0.0f);
		v.color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		v.uv = Vector2::Zero;
	}
	m_VertexBuffer.Create(m_Vertices);

	m_Shader.Create("shader/longBeamVS.hlsl", "shader/longBeamPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(Params));

	// 線に沿って流れる模様に使う。無くても描画はできる
	m_NoiseLoaded = m_Noise.Load("assets/texture/noise.png");
}

void LongBeamRibbon::Draw(Camera* cam, const std::vector<Point>& points, float width, float intensity,
	float time, const Vector3& glowColor)
{
	int n = std::min((int)points.size(), kMaxPoints);
	if (n < 2) return;

	const Vector3 camPos = cam->GetPosition();
	float v = 0.0f;	// 線に沿った距離（幅1つ分で1進む）

	for (int i = 0; i < n; i++)
	{
		// この点で線が進む向き（前後の点から求める）
		Vector3 a = points[(i > 0) ? i - 1 : i].pos;
		Vector3 b = points[(i < n - 1) ? i + 1 : i].pos;
		Vector3 dir = b - a;
		if (dir.LengthSquared() < 0.000001f) dir = Vector3(0.0f, 0.0f, 1.0f);
		dir.Normalize();

		// 線に垂直で、カメラから見て横に広がる向き（外積）
		Vector3 side = dir.Cross(camPos - points[i].pos);
		if (side.LengthSquared() < 0.000001f) side = Vector3(1.0f, 0.0f, 0.0f);
		side.Normalize();
		side *= width * 0.5f;

		if (i > 0) v += (points[i].pos - points[i - 1].pos).Length() / width;

		VERTEX_3D& L = m_Vertices[i * 2 + 0];
		VERTEX_3D& R = m_Vertices[i * 2 + 1];
		L.position = points[i].pos - side;
		R.position = points[i].pos + side;
		L.color = R.color = points[i].color;
		L.uv = Vector2(0.0f, v);
		R.uv = Vector2(1.0f, v);
	}
	m_VertexBuffer.Modify(m_Vertices);

	// 頂点はもうワールド座標なので、World行列は単位行列
	Matrix world = Matrix::Identity;
	Renderer::SetWorldMatrix(&world);

	Params p = {};
	p.glowColor = Vector4(glowColor.x, glowColor.y, glowColor.z, 1.0f);
	p.intensity = intensity;
	p.time = time;
	m_Shader.SetGPU();
	m_Shader.UpdatePSConstantBuffer(&p, sizeof(p));
	if (m_NoiseLoaded) m_Noise.SetGPU(0);
	m_VertexBuffer.SetGPU();

	ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Renderer::SetCullMode(CULL_NONE);		// 帯の裏表を気にしない
	Renderer::SetDepthEnable(false);		// 深度テストはするが書き込まない（半透明の光なので）
	ctx->Draw(n * 2, 0);
	Renderer::SetDepthEnable(true);
	Renderer::SetCullMode(CULL_BACK);
}
