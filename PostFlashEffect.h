#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>

using Microsoft::WRL::ComPtr;

class PostFlashEffect
{
private:
	ComPtr<ID3D11Texture2D> m_SceneTex;
	ComPtr<ID3D11RenderTargetView> m_SceneRTV;
	ComPtr<ID3D11ShaderResourceView> m_SceneSRV;

	ComPtr<ID3D11VertexShader> m_VS;
	ComPtr<ID3D11PixelShader> m_PS;
	ComPtr<ID3D11InputLayout>  m_Layout;

	ComPtr<ID3D11Buffer> m_FlashCB;
	ComPtr<ID3D11SamplerState> m_Sampler;

	int m_Width = 0;
	int m_Height = 0;

public:
	void Init(int width, int height);
	void BeginScene();
	void EndSceneAndApply(float flashAlpha);
};

