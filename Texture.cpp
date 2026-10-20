#include	<iostream>
#include    <unordered_map>
#include	"Texture.h"
#include	"stb_image.h"
#include	"renderer.h"

namespace
{
	struct CachedTexture
	{
		ComPtr<ID3D11ShaderResourceView> srv;
		int width, height, bpp;
	};
	std::unordered_map<std::string, CachedTexture> g_TextureCache;
}

// テクスチャをロード
bool Texture::Load(const std::string& filename)
{
	//前に読んだことがあればおなじものを共有
	auto cached = g_TextureCache.find(filename);
	if (cached != g_TextureCache.end())
	{
		m_srv = cached->second.srv;
		m_width = cached->second.width;
		m_height = cached->second.height;
		m_bpp = cached->second.bpp;
		m_texname = filename;
		return true;
	}

	bool sts = true;
	unsigned char* pixels;

	// 画像読み込み
	pixels = stbi_load(filename.c_str(), &m_width, &m_height, &m_bpp, 4);
	if (pixels == nullptr) {
		std::cout << filename.c_str() << " Load error " << std::endl;
		return false;
	}

	// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	// テクスチャ解放
	//pTexture->Release();

	g_TextureCache[filename] = { m_srv, m_width, m_height, m_bpp };
	return true;
}

// テクスチャをメモリからロード
bool Texture::LoadFromFemory(const unsigned char* Data,int len) {

	bool sts = true;
	unsigned char* pixels;

	// 画像読み込み
	pixels = stbi_load_from_memory(Data, 
		len, 
		&m_width, 
		&m_height, 
		&m_bpp, 
		STBI_rgb_alpha);

	// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	return true;
}

// テクスチャをGPUにセット
//void Texture::SetGPU()
//{
//	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
//	devicecontext->PSSetShaderResources(0, 1, m_srv.GetAddressOf());
//}

void Texture::SetGPU(UINT slot)
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->PSSetShaderResources(slot, 1, m_srv.GetAddressOf());
}
