#include "Billboard.h"
#include "DirectX.h"

Billboard::Data Billboard::m_data;
std::shared_ptr<Texture> Billboard::m_whiteTex;

void Billboard::Init()
{
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};

	const char* VS = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};
cbuffer Matrix : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos = mul(vout.pos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.uv = vin.uv;
	return vout;
})EOT";
	const char* PS = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};
cbuffer Param : register(b0) {
	float4 color;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET0 {
	return tex.Sample(samp, pin.uv) * color;
})EOT";

	m_data.defVS = std::make_shared<VertexShader>();
	m_data.defVS->Compile(VS);
	m_data.defPS = std::make_shared<PixelShader>();
	m_data.defPS->Compile(PS);

	const float d = 0.5f;
	Vertex vtx[] = {
		{DirectX::XMFLOAT3(-d,  d, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f)},
		{DirectX::XMFLOAT3( d,  d, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f)},
		{DirectX::XMFLOAT3(-d, -d, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f)},
		{DirectX::XMFLOAT3( d, -d, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f)},
	};
	WORD idx[] = {
		2, 0, 1, 1, 3, 2,
	};
	MeshBuffer::Description desc = {};
	desc.pVtx = vtx;
	desc.vtxSize = sizeof(Vertex);
	desc.vtxCount = _countof(vtx);
	desc.pIdx = idx;
	desc.idxSize = sizeof(idx[0]);
	desc.idxCount = _countof(idx);
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_data.mesh = std::make_shared<MeshBuffer>(desc);

	for (int i = 0; i < 3; ++i)
		DirectX::XMStoreFloat4x4(&m_data.matrix[i], DirectX::XMMatrixIdentity());
	m_data.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_data.blendAdd = false;

	BYTE white[] = { 255,255,255,255 };
	m_whiteTex = std::make_shared<Texture>();
	m_whiteTex->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, white);
	m_data.texture = m_whiteTex.get();
}

void Billboard::Uninit()
{
	m_whiteTex.reset();
	m_data.mesh.reset();
	m_data.defPS.reset();
	m_data.defVS.reset();
}

void Billboard::Draw()
{
	DirectX::XMMATRIX world = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&m_data.matrix[0]));
	DirectX::XMMATRIX view = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&m_data.matrix[1]));

	DirectX::XMVECTOR scale, rot, trans;
	DirectX::XMMatrixDecompose(&scale, &rot, &trans, world);

	DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, view);
	DirectX::XMFLOAT4X4 iv;
	DirectX::XMStoreFloat4x4(&iv, invView);

	DirectX::XMMATRIX billboardRot = DirectX::XMMatrixSet(
		iv._11, iv._12, iv._13, 0.0f,
		iv._21, iv._22, iv._23, 0.0f,
		iv._31, iv._32, iv._33, 0.0f,
		0.0f,   0.0f,   0.0f,   1.0f
	);

	DirectX::XMMATRIX bbWorld =
		DirectX::XMMatrixScalingFromVector(scale) *
		billboardRot *
		DirectX::XMMatrixTranslationFromVector(trans);

	DirectX::XMFLOAT4X4 matrix[3];
	DirectX::XMStoreFloat4x4(&matrix[0], DirectX::XMMatrixTranspose(bbWorld));
	matrix[1] = m_data.matrix[1];
	matrix[2] = m_data.matrix[2];

	m_data.defVS->WriteBuffer(0, matrix);
	m_data.defPS->WriteBuffer(0, &m_data.color);
	m_data.defVS->Bind();
	m_data.defPS->SetTexture(0, m_data.texture);
	m_data.defPS->Bind();

	// 半透明描画のため深度テストのみ行い書き込みは行わない(奥のオブジェクトを遮らないようにする)
	SetDepthTest(DEPTH_ENABLE_TEST);
	if (m_data.blendAdd) SetBlendMode(BLEND_ADD);
	m_data.mesh->Draw();
	if (m_data.blendAdd) SetBlendMode(BLEND_ALPHA);
	SetDepthTest(DEPTH_ENABLE_WRITE_TEST);
}

void Billboard::SetWorld(DirectX::XMFLOAT4X4 world)
{
	m_data.matrix[0] = world;
}
void Billboard::SetView(DirectX::XMFLOAT4X4 view)
{
	m_data.matrix[1] = view;
}
void Billboard::SetProjection(DirectX::XMFLOAT4X4 proj)
{
	m_data.matrix[2] = proj;
}
void Billboard::SetColor(DirectX::XMFLOAT4 color)
{
	m_data.color = color;
}
void Billboard::SetTexture(Texture* tex)
{
	m_data.texture = tex ? tex : m_whiteTex.get();
}
void Billboard::SetBlendAdd(bool enable)
{
	m_data.blendAdd = enable;
}
