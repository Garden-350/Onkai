#include "LightShaft.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

LightShaft::LightShaft(ID3D11Device* device)
{
    ZeroMemory(&m_constants, sizeof(m_constants));
    m_constants.intensity = 1.0f;
    m_constants.falloff = 1.5f;
    m_constants.samplingDensity = 32.0f;
}

LightShaft::~LightShaft()
{
}

bool LightShaft::Initialize(ID3D11Device* device)
{
    if (!CreateShaders(device))
        return false;
    if (!CreateScreenQuad(device))
        return false;
    if (!CreateConstantBuffer(device))
        return false;

    // サンプラーステート作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf())))
        return false;

    // ブレンドステート作成（加算合成）
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(device->CreateBlendState(&blendDesc, m_blendState.GetAddressOf())))
        return false;

    return true;
}

bool LightShaft::CreateShaders(ID3D11Device* device)
{
    // 頂点シェーダーのコンパイル
    ComPtr<ID3DBlob> vsBlob;
    HRESULT hr = D3DCompileFromFile(
        L"shader/lightShaftVS.hlsl",
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        vsBlob.GetAddressOf(),
        nullptr
    );

    if (FAILED(hr))
    {
        // ファイルが見つからない場合、空のシェーダーをコンパイル
        const char* vsSource = R"(
            cbuffer LightShaftCB : register(b0)
            {
                float4 lightOrigin;
                float4 lightDirection;
                float intensity;
                float falloff;
                float samplingDensity;
                float padding;
            };

            struct VS_INPUT
            {
                float3 position : POSITION;
                float2 texCoord : TEXCOORD0;
            };

            struct PS_INPUT
            {
                float4 position : SV_POSITION;
                float2 texCoord : TEXCOORD0;
            };

            PS_INPUT main(VS_INPUT input)
            {
                PS_INPUT output;
                output.position = float4(input.position, 1.0f);
                output.texCoord = input.texCoord;
                return output;
            }
        )";

        if (FAILED(D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), nullptr)))
            return false;
    }

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.GetAddressOf())))
        return false;

    // 入力レイアウト作成
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout.GetAddressOf())))
        return false;

    // ピクセルシェーダーのコンパイル
    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompileFromFile(
        L"shader/lightShaftPS.hlsl",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        psBlob.GetAddressOf(),
        nullptr
    );

    if (FAILED(hr))
    {
        // ファイルが見つからない場合、デフォルトシェーダーをコンパイル
        const char* psSource = R"(
            cbuffer LightShaftCB : register(b0)
            {
                float4 lightOrigin;
                float4 lightDirection;
                float intensity;
                float falloff;
                float samplingDensity;
                float padding;
            };

            Texture2D mainTexture : register(t0);
            SamplerState mainSampler : register(s0);

            struct PS_INPUT
            {
                float4 position : SV_POSITION;
                float2 texCoord : TEXCOORD0;
            };

            float4 main(PS_INPUT input) : SV_TARGET
            {
                float2 uv = input.texCoord;
                
                // ライトシャフトの方向
                float2 lightDir = normalize(lightOrigin.xy - uv);
                
                // サンプリング
                float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
                int samples = (int)samplingDensity;
                float step = 1.0f / (float)samples;
                
                for (int i = 0; i < samples; i++)
                {
                    float2 samplePos = uv + lightDir * step * (float)i;
                    samplePos = clamp(samplePos, 0.0f, 1.0f);
                    
                    float4 sample = mainTexture.Sample(mainSampler, samplePos);
                    float dist = length(samplePos - lightOrigin.xy);
                    float falloffFactor = exp(-dist * dist * falloff);
                    
                    result += sample * falloffFactor * (1.0f / (float)samples);
                }
                
                return result * intensity;
            }
        )";

        if (FAILED(D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, psBlob.GetAddressOf(), nullptr)))
            return false;
    }

    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.GetAddressOf())))
        return false;

    return true;
}

bool LightShaft::CreateScreenQuad(ID3D11Device* device)
{
    // スクリーン全体を覆うクワッド
    ScreenQuadVertex vertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&bufferDesc, &initData, m_screenQuadVB.GetAddressOf())))
        return false;

    return true;
}

bool LightShaft::CreateConstantBuffer(ID3D11Device* device)
{
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(LightShaftConstants);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, m_constantBuffer.GetAddressOf())))
        return false;

    return true;
}

void LightShaft::SetLightOrigin(float screenX, float screenY)
{
    m_constants.lightOrigin = XMFLOAT4(screenX, screenY, 0.0f, 0.0f);
}

void LightShaft::SetIntensity(float intensity)
{
    m_constants.intensity = intensity;
}

void LightShaft::SetFalloff(float falloff)
{
    m_constants.falloff = falloff;
}

void LightShaft::SetSamplingDensity(float density)
{
    m_constants.samplingDensity = density;
}

void LightShaft::Render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* sceneTexture)
{
    // 定数バッファを更新
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &m_constants, sizeof(m_constants));
        context->Unmap(m_constantBuffer.Get(), 0);
    }

    // シェーダーを設定
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->IASetInputLayout(m_inputLayout.Get());

    // 定数バッファ、テクスチャ、サンプラーを設定
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, &sceneTexture);
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // ブレンドステートを設定
    float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);

    // 頂点バッファを設定
    UINT stride = sizeof(ScreenQuadVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_screenQuadVB.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 描画
    context->Draw(4, 0);
}

