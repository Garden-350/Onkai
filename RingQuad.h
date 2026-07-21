//
// RingQuad.h
//
// 3Dワールド空間(XZ平面に寝かせた1枚のクワッド)にテクスチャを貼るだけの
// 軽量クラス。ノーツの「接近リング(判定円に近づくほど大きくなる的)」に使う。
// Texture2Dと違い、カメラモードは切り替えない（呼び出し側が3Dカメラのまま使う）。
//
#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Material.h"
#include "Texture.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <memory>
#include <vector>

class RingQuad
{
public:
    // texturePath : リング画像のパス（例: "assets/texture/ring.png"）
    void Init(const char* texturePath);

    // center : ワールド座標（クワッドの中心）
    // scale  : クワッドの一辺の大きさ
    // alpha  : 不透明度(0〜1)。呼び出し側で Renderer::SetBlendState(BS_ALPHABLEND) 済みであること
    void Draw(const DirectX::SimpleMath::Vector3& center, float scale, float alpha);

private:
    Shader m_Shader;
    std::unique_ptr<Material> m_Material;
    Texture m_Texture;
    VertexBuffer<VERTEX_3D> m_VertexBuffer;
    IndexBuffer m_IndexBuffer;
};
