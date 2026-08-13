#include	"Shader.h"
#include	"renderer.h"

//=======================================
//Shader作成
//=======================================
void Shader::Create(std::string vs, std::string ps)
{
	// 頂点データの定義
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,   D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	unsigned int numElements = ARRAYSIZE(layout);

	ID3D11Device* device = Renderer::GetDevice();

	// 頂点シェーダーオブジェクトを生成、同時に頂点レイアウトも生成
	HRESULT hr = Renderer::CreateVertexShader(
		&m_pVertexShader,
		&m_pVertexLayout,
		layout,
		numElements,
		vs.c_str()
		);
	if (FAILED(hr)) {
		MessageBox(nullptr, "CreateVertexShader error", "error", MB_OK);
		return;
	}

	// ピクセルシェーダーを生成

	hr = Renderer::CreatePixelShader(			// ピクセルシェーダーオブジェクトを生成
		&m_pPixelShader,
		ps.c_str()
		);
	if (FAILED(hr)) {
		MessageBox(nullptr, "CreatePixelShader error", "error", MB_OK);
		return;
	}

	return;
}

//=======================================
//GPUにデータを送る
//=======================================
void Shader::SetGPU()
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();

	devicecontext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);		// 頂点シェーダーをセット
	devicecontext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);		// ピクセルシェーダーをセット
	devicecontext->IASetInputLayout(m_pVertexLayout.Get());				// 頂点レイアウトセット
}

void Shader::CreatePSConstantBuffer(UINT byteSize)
{
	ID3D11Device* device = Renderer::GetDevice();
	if (device == nullptr)
	{
		OutputDebugStringA("[CreatePSConstantBuffer] device is nullptr!\n");
		return;
	}
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = byteSize;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = device->CreateBuffer(&bd, nullptr, m_pPSConstantBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		char buf[256];
		sprintf_s(buf, "CreatePSConstantBuffer error: hr=0x%08X, byteSize=%u\n", hr, byteSize);
		OutputDebugStringA(buf);
		MessageBox(nullptr, "CreatePSConstantBuffer error", "error", MB_OK);
	}
}

void Shader::UpdatePSConstantBuffer(const void* data, UINT byteSize)
{
	if (!m_pPSConstantBuffer) return;

	ID3D11DeviceContext* dc = Renderer::GetDeviceContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = dc->Map(m_pPSConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (SUCCEEDED(hr))
	{
		memcpy(mapped.pData, data, byteSize);
		dc->Unmap(m_pPSConstantBuffer.Get(), 0);
	}

	dc->PSSetConstantBuffers(6, 1, m_pPSConstantBuffer.GetAddressOf());
}

