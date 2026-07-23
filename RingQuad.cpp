#include "RingQuad.h"

using namespace DirectX::SimpleMath;

void RingQuad::Init(const char* texturePath)
{
    // XY平面上の正方形（Texture2Dと同じ向き、Draw時のscaleで拡縮する）
    std::vector<VERTEX_3D> verts(4);
    verts[0].position = Vector3(-0.5f,  0.5f, 0.0f);
    verts[1].position = Vector3( 0.5f,  0.5f, 0.0f);
    verts[2].position = Vector3(-0.5f, -0.5f, 0.0f);
    verts[3].position = Vector3( 0.5f, -0.5f, 0.0f);

    for (int i = 0; i < 4; i++)
    {
        verts[i].normal = Vector3(0.0f, 0.0f, -1.0f);
        verts[i].color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    }
    verts[0].uv = Vector2(0.0f, 0.0f);
    verts[1].uv = Vector2(1.0f, 0.0f);
    verts[2].uv = Vector2(0.0f, 1.0f);
    verts[3].uv = Vector2(1.0f, 1.0f);

    m_VertexBuffer.Create(verts);

    std::vector<unsigned int> idx = { 0, 1, 2, 3 };
    m_IndexBuffer.Create(idx);

    // テクスチャありの単色シェーダーを流用（Texture2Dと同じもの）
    m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");

    MATERIAL mtrl{};
    mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
    mtrl.TextureEnable = TRUE;
    m_Material = std::make_unique<Material>();
    m_Material->Create(mtrl);

    m_Texture.Load(texturePath);
}

void RingQuad::Draw(const Vector3& center, float scale, float alpha)
{
    Matrix world = Matrix::CreateScale(scale) * Matrix::CreateTranslation(center);
    Renderer::SetWorldMatrix(&world);

    m_Shader.SetGPU();
    m_VertexBuffer.SetGPU();
    m_IndexBuffer.SetGPU();
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 不透明度をalphaに差し替えてマテリアルを更新
    m_Material->SetDiffuse(Color(1.0f, 1.0f, 1.0f, alpha));
    m_Material->Update();

    Renderer::SetUV(0.0f, 0.0f, 1.0f, 1.0f); // UV分割なし
    m_Texture.SetGPU();
    m_Material->SetGPU();

    Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}
