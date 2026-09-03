#include "BarrierQuad.h"

using namespace DirectX::SimpleMath;

void BarrierQuad::Init()
{
    // ── 板ポリ頂点（中心原点、XY平面）──
    std::vector<VERTEX_3D> vertices =
    {
        { {-0.5f,  0.5f, 0.0f}, {0,0,1}, {1,1,1,1}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f}, {0,0,1}, {1,1,1,1}, {1.0f, 0.0f} },
        { {-0.5f, -0.5f, 0.0f}, {0,0,1}, {1,1,1,1}, {0.0f, 1.0f} },
        { { 0.5f, -0.5f, 0.0f}, {0,0,1}, {1,1,1,1}, {1.0f, 1.0f} },
    };
    std::vector<unsigned int> indices = { 0,1,2, 2,1,3 };
    m_IndexCount = (unsigned int)indices.size();

    m_VertexBuffer.Create(vertices);
    m_IndexBuffer.Create(indices);

    // ── シェーダー作成（既存Shaderクラスと同じ使い方）──
    m_Shader.Create("shader/barrierVS.hlsl", "shader/barrierPS.hlsl");
    m_Shader.CreatePSConstantBuffer(sizeof(BarrierParms));
}

void BarrierQuad::Draw(const Vector3& center, float scale,
    float progress, const Vector3& baseColor, const Matrix& view)
{
	Renderer::SetCullMode(CULL_NONE);

    // ── ビルボード化：View行列からright/upベクトルを取り出す ──
    Vector3 right(view._11, view._21, view._31);
    Vector3 up(view._12, view._22, view._32);

    Matrix world =
        Matrix::CreateScale(scale, scale, 1.0f) *
        Matrix(
            right.x, right.y, right.z, 0,
            up.x, up.y, up.z, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        ) *
        Matrix::CreateTranslation(center);

    Matrix worldCopy = world;
    Renderer::SetWorldMatrix(&worldCopy);

    // ── シェーダーセット＋定数バッファ更新 ──
    m_Shader.SetGPU();

    BarrierParms parms = {};
    parms.progress = progress;
    parms.baseColor = Vector4(baseColor.x, baseColor.y, baseColor.z, 1.0f);
    m_Shader.UpdatePSConstantBuffer(&parms, sizeof(parms));

    // ── 頂点/インデックスバインド ──
    m_VertexBuffer.SetGPU();
    m_IndexBuffer.SetGPU();

    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Renderer::GetDeviceContext()->DrawIndexed(m_IndexCount, 0, 0);
	Renderer::SetCullMode(CULL_BACK);
}