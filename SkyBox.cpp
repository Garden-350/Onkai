#include "SkyBox.h"
#include "Game.h"

using namespace DirectX::SimpleMath;

void SkyBox::Init()
{
	// right, left, top, bottom, front(+Z), back(-Z)
	const char* texturePaths[6] =
	{
		"assets/texture/skybox/py.jpg",
		"assets/texture/skybox/py.jpg",
		"assets/texture/skybox/py.jpg",
		"assets/texture/skybox/ny.jpg",
		"assets/texture/skybox/py.jpg",
		"assets/texture/skybox/py.jpg",
	};

	for (size_t i = 0; i < m_Textures.size(); ++i)
	{
		m_TextureLoaded[i] = m_Textures[i].Load(texturePaths[i]);
	}

	const float h = 0.5f;

	auto addFace = [this](
		const Vector3& a,
		const Vector3& b,
		const Vector3& c,
		const Vector3& d
		)
		{
			const unsigned int base =
				static_cast<unsigned int>(m_Vertices.size());

			const Vector3 positions[4] = { a, b, c, d };
			const Vector2 uv[4] =
			{
				{0,0},
				{1,0},
				{1,1},
				{0,1}
			};

			for (int i = 0; i < 4; ++i)
			{
				VERTEX_3D vertex{};
				vertex.position = positions[i];
				vertex.color = Color(1, 1, 1, 1);
				vertex.uv = uv[i];
				m_Vertices.emplace_back(vertex);
			}

			m_Indices.insert(
				m_Indices.end(),
				{
					base, base + 1, base + 2,
					base, base + 2, base + 3
				}
			);
		};

	// right
	addFace(
		Vector3(h, h, -h),
		Vector3(h, h, h),
		Vector3(h, -h, h),
		Vector3(h, -h, -h)
	);
	//left
	addFace(
		Vector3(-h, h, h),
		Vector3(-h, h, -h),
		Vector3(-h, -h, -h),
		Vector3(-h, -h, h)
	);
	//top
	addFace(
		Vector3(-h, h, -h),
		Vector3(-h, h, h),
		Vector3(h, h, h),
		Vector3(h, h, -h)
	);
	//bottom
	addFace(
		Vector3(-h, -h, h),
		Vector3(-h, -h, -h),
		Vector3(h, -h, -h),
		Vector3(h, -h, h)
	);
	//front(+Z)
	addFace(
		Vector3(-h, h, h),
		Vector3(-h, -h, h),
		Vector3(h, -h, h),
		Vector3(h, h, h)
	);
	//back
	addFace(
		Vector3(h, h, -h),
		Vector3(h, -h, -h),
		Vector3(-h, -h, -h),
		Vector3(-h, h, -h)
	);

	m_VertexBuffer.Create(m_Vertices);
	m_IndexBuffer.Create(m_Indices);

	//‹ó‚ÍÆ–¾‚Ì‰e‹¿‚ðŽó‚¯‚È‚¢
	m_Shader.Create(
		"shader/unlitTextureVS.hlsl",
		"shader/unlitTexturePS.hlsl"
	);

	//” ‚Ì“à‘¤‚¾‚¯•`‰æ‚·‚é
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_FRONT;
	rasterizerDesc.DepthClipEnable = TRUE;

	Renderer::GetDevice()->CreateRasterizerState(
		&rasterizerDesc,
		m_InsideRasterizer.GetAddressOf()
	);
}

void SkyBox::Update()
{
	m_Position = Game::GetInstance()->GetCamera()->GetPosition();
}

void SkyBox::Draw(Camera* cam)
{
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	// ‹ó‚ªŽè‘O‚Ì•¨‘Ì‚ð‰B‚³‚È‚¢‚æ‚¤A[“x‚ð‘‚«ž‚Ü‚È‚¢
	Renderer::SetDepthEnable(false);

	Matrix world =
		Matrix::CreateScale(m_Size) *
		Matrix::CreateTranslation(m_Position);

	Renderer::SetWorldMatrix(&world);

	ID3D11DeviceContext* deviceContext =
		Renderer::GetDeviceContext();

	deviceContext->RSSetState(m_InsideRasterizer.Get());
	deviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	m_Shader.SetGPU();
	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();

	Renderer::SetUV(0, 0, 1, 1);

	for (size_t i = 0; i < m_Textures.size(); ++i)
	{
		if (!m_TextureLoaded[i])
		{
			continue;
		}

		m_Textures[i].SetGPU();

		// 1–Ê‚É‚Â‚«ƒCƒ“ƒfƒbƒNƒX6ŒÂ
		deviceContext->DrawIndexed(
			6,
			static_cast<UINT>(i * 6),
			0
		);
	}

	// ’Êí‚Ì”w–ÊƒJƒŠƒ“ƒO‚Ö–ß‚·
	deviceContext->RSSetState(nullptr);

	Renderer::SetDepthEnable(true);
}

void SkyBox::Uninit()
{
}
