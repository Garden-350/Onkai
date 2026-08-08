#include "PostFlashEffect.h"
#include "Renderer.h"

struct FlashCBData
{
	DirectX::SimpleMath::Vector4 flashColor; // rgb + 未使用
	float flashAlpha;
	float pad[3];
};

void PostFlashEffect::Init(int width, int height)
{
	m_Width = width;
	m_Height = height;

	auto device = Renderer::GetDevice();


	// ── オフスクリーンのシーン用テクスチャ ──
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	device->CreateTexture2D(&desc, nullptr, m_SceneTex.GetAddressOf());
	device->CreateRenderTargetView(m_SceneTex.Get(), nullptr, m_SceneRTV.GetAddressOf());
	device->CreateShaderResourceView(m_SceneTex.Get(), nullptr, m_SceneSRV.GetAddressOf());

	HRESULT hrVS = Renderer::CreateVertexShader(
		m_VS.GetAddressOf(), m_Layout.GetAddressOf(),
		nullptr, 0,
		"shader/FullscreenVS.hlsl"
	);
	if (FAILED(hrVS))
		OutputDebugStringA("[PostFlashEffect] FullscreenVS.hlsl load FAILED\n");
	else
		OutputDebugStringA("[PostFlashEffect] FullscreenVS.hlsl load OK\n");

	HRESULT hrPS = Renderer::CreatePixelShader(m_PS.GetAddressOf(), "shader/PostFlashPS.hlsl");
	if (FAILED(hrPS))
		OutputDebugStringA("[PostFlashEffect] PostFlashPS.hlsl load FAILED\n");
	else
		OutputDebugStringA("[PostFlashEffect] PostFlashPS.hlsl load OK\n");

	// ── シェーダのロード（Rendererの既存ヘルパーを使う）──
	// フルスクリーン三角形は入力レイアウト不要な構成にしたいが、
	// CreateVertexShaderはD3D11_INPUT_ELEMENT_DESC配列を要求するので、
	// ダミーで空配列（numElements=0）を渡す
	Renderer::CreateVertexShader(
		m_VS.GetAddressOf(), m_Layout.GetAddressOf(),
		nullptr, 0,
		"shader/FullscreenVS.hlsl"
	);
	Renderer::CreatePixelShader(m_PS.GetAddressOf(), "shader/PostFlashPS.hlsl");

	// ── 定数バッファ ──
	Renderer::CreateConstantBufferWrite(sizeof(FlashCBData), m_FlashCB.GetAddressOf());

	// ── サンプラー ──
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, m_Sampler.GetAddressOf());
}

void PostFlashEffect::BeginScene()
{
	auto context = Renderer::GetDeviceContext();

	float clearColor[4] = { 0.39f, 0.58f, 0.93f, 1.0f }; // CornflowerBlue相当
	context->ClearRenderTargetView(m_SceneRTV.Get(), clearColor);

	context->ClearDepthStencilView(Renderer::GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// depthは既存のRendererのDSVをそのまま使い回す
	context->OMSetRenderTargets(1, m_SceneRTV.GetAddressOf(), Renderer::GetDepthStencilView());

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)m_Width;
	vp.Height = (float)m_Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);
}

void PostFlashEffect::EndSceneAndApply(float flashAlpha)
{
	auto context = Renderer::GetDeviceContext();

	// ── 定数バッファ更新 ──
	D3D11_MAPPED_SUBRESOURCE mapped;
	context->Map(m_FlashCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	FlashCBData cbData;
	cbData.flashColor = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白
	cbData.flashAlpha = flashAlpha;
	memcpy(mapped.pData, &cbData, sizeof(cbData));
	context->Unmap(m_FlashCB.Get(), 0);

	// ── 描画先をバックバッファへ戻す ──
	Renderer::SetRenderTarget(Renderer::GetBackBufferRTV(), Renderer::GetDepthStencilView());

	// フラッシュ無し(alpha<=0)ならシーンテクスチャをそのまま転送するだけでもOKだが、
	// シェーダ側でalpha=0なら素通しになるようにしているのでそのまま呼んでよい
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer* nullVB = nullptr;
	UINT stride = 0, offset = 0;
	context->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset); // 頂点バッファ無し（VS内でIDから生成）

	context->VSSetShader(m_VS.Get(), nullptr, 0);
	context->PSSetShader(m_PS.Get(), nullptr, 0);
	context->PSSetShaderResources(0, 1, m_SceneSRV.GetAddressOf());
	context->PSSetSamplers(1, 1, m_Sampler.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, m_FlashCB.GetAddressOf());

	context->Draw(3, 0);

	// 後片付け：SRVをアンバインド（同じテクスチャを次フレームRTVとして使うため必須）
	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(0, 1, &nullSRV);
}
