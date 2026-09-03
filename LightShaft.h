#pragma once
#include <DirectXMath.h>
#include <wrl.h>
#include <d3d11.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class LightShaft
{
public:
    struct LightShaftConstants
    {
        XMFLOAT4 lightOrigin;      // ライトシャフトの発射位置（スクリーン座標系）
        XMFLOAT4 lightDirection;   // ライトの方向（正規化される）
        float intensity;            // シャフトの強度
        float falloff;              // フェードアウト
        float samplingDensity;      // サンプリング密度
        float padding;
    };

    struct ScreenQuadVertex
    {
        XMFLOAT3 position;
        XMFLOAT2 texCoord;
    };

public:
    LightShaft(ID3D11Device* device);
    ~LightShaft();

    // 初期化
    bool Initialize(ID3D11Device* device);

    // ライトシャフトの設定
    void SetLightOrigin(float screenX, float screenY);  // スクリーン座標[0-1]
    void SetIntensity(float intensity);
    void SetFalloff(float falloff);
    void SetSamplingDensity(float density);

    // レンダリング
    void Render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* sceneTexture);

private:
    bool CreateShaders(ID3D11Device* device);
    bool CreateScreenQuad(ID3D11Device* device);
    bool CreateConstantBuffer(ID3D11Device* device);

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;

    ComPtr<ID3D11Buffer> m_screenQuadVB;
    ComPtr<ID3D11Buffer> m_constantBuffer;

    LightShaftConstants m_constants;

    ComPtr<ID3D11SamplerState> m_samplerState;
    ComPtr<ID3D11BlendState> m_blendState;
};

