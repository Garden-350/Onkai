#include "Oto.h"
#include "Renderer.h"
#include <algorithm>
#undef min

void Oto::Init()
{
	m_Shader.Create("shader/litTextureVS.hlsl", "shader/litTexturePS.hlsl");
}

bool Oto::Load(const std::string& file, const std::string& texDir)
{
	StaticMesh mesh;
	mesh.Load(file, texDir);
	if (mesh.GetSubsets().empty())
	{
		OutputDebugStringA(("[Oto] Failed to load " + file + "\n").c_str());
		return false;
	}

	m_MeshRenderer.Init(mesh);
	m_Subsets = mesh.GetSubsets();
	m_Textures = mesh.GetTextures();
	for (auto& m : mesh.GetMaterials())
	{
		auto material = std::make_unique<Material>();
		material->Create(m);
		m_Materials.emplace_back(std::move(material));
	}

	//この部品の真ん中を測る（ここを中心に自分で回る）
	const auto& verts = mesh.GetVertices();
	Vector3 mn = verts[0].position, mx = verts[0].position;
	for (const auto& v : verts)
	{
		mn = Vector3::Min(mn, v.position);
		mx = Vector3::Max(mx, v.position);
	}
	m_Pivot = (mn + mx) * 0.5f;
	if (!m_HasOffset) m_Offset = m_Pivot - s_LogoAxis;
	return true;
}

bool Oto::SetTexture(const std::string& file)
{
	auto tex = std::make_unique<Texture>();
	if (!tex->Load(file))
	{
		OutputDebugStringA(("[Oto] Failed to load texture " + file + "\n").c_str());
		return false;
	}
	m_OverrideTexture = std::move(tex);

	for (auto& m : m_Materials)
	{
		m->SetTextureEnable(true);
		m->Update();
	}
	return true;
}

bool Oto::SetMatcap(const std::string& file)
{
	auto tex = std::make_unique<Texture>();
	if (!tex->Load(file))
	{
		OutputDebugStringA(("[Oto] Failed to load matcap " + file + "\n").c_str());
		return false;
	}
	m_Matcap = std::move(tex);
	m_Shader.Create("shader/matcapVS.hlsl", "shader/matcapPS.hlsl");	//シェーダーをマットキャップ用に差し替え
	return true;
}

void Oto::Update()
{
	const auto now = std::chrono::steady_clock::now();
	if (!m_Started) { m_LastUpdateClock = now; m_Started = true; }
	float dt = std::chrono::duration<float>(now - m_LastUpdateClock).count();
	m_LastUpdateClock = now;
	dt = std::min(dt, 0.1f);

	m_Rotation += m_SpinSpeed * dt;	//自分の真ん中で回す角度
}

void Oto::Draw(Camera* cam)
{
	if (m_Subsets.empty()) return;

	cam->SetCamera(0);
	Renderer::SetBlendState(BS_NONE);

	Matrix own = Matrix::CreateTranslation(-m_Pivot)
		* Matrix::CreateRotationX(m_Rotation.x) * Matrix::CreateRotationY(m_Rotation.y) * Matrix::CreateRotationZ(m_Rotation.z)
		* Matrix::CreateTranslation(m_Offset);
	Matrix logo = Matrix::CreateScale(s_LogoScale)
		* Matrix::CreateRotationX(s_LogoRotation.x) * Matrix::CreateRotationY(s_LogoRotation.y) * Matrix::CreateRotationZ(s_LogoRotation.z)
		* Matrix::CreateTranslation(s_LogoPosition);
	Matrix world = own * logo;
	Renderer::SetWorldMatrix(&world);

	m_Shader.SetGPU();
	m_MeshRenderer.BeforeDraw();
	for (size_t i = 0; i < m_Subsets.size(); i++)
	{
		unsigned int idx = m_Subsets[i].MaterialIdx;
		m_Materials[idx]->SetGPU();

		if (m_Matcap)
			m_Matcap->SetGPU();			//マットキャップ（金の球）
		else if (m_OverrideTexture)
			m_OverrideTexture->SetGPU();	//コードで貼った画像
		else if (m_Materials[idx]->isTextureEnable() && idx < m_Textures.size())
			m_Textures[idx]->SetGPU();		//モデルに入っていた画像

		m_MeshRenderer.DrawSubset(m_Subsets[i].IndexNum, m_Subsets[i].IndexBase, m_Subsets[i].VertexBase);
	}
}

void Oto::Uninit()
{
}