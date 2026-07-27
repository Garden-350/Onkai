#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Material.h"
#include "Texture.h"
#include "Renderer.h"
#include "Camera.h"
#include <SimpleMath.h>
#include <memory>
#include <vector>

class ParticleQuad
{
public:
	void Init(const char* texturePath);
    // cam    : ビルボード回転の計算に使うカメラ
    // center : ワールド座標（クワッドの中心）
    // scale  : クワッドの一辺の大きさ
    // color  : 乗算する色(RGBA)。呼び出し側で必要なブレンドステート(加算合成など)を設定しておくこと
    void Draw(Camera* cam, const DirectX::SimpleMath::Vector3& center,
        float scale, const DirectX::SimpleMath::Color& color);

private:
    Shader m_Shader;
    std::unique_ptr<Material> m_Material;
    Texture m_Texture;
    VertexBuffer<VERTEX_3D> m_VertexBuffer;
    IndexBuffer m_IndexBuffer;
};

