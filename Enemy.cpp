#include "Enemy.h"
#include "Game.h"
#undef max
#undef min

void Enemy::Init()
{
	StaticMesh mesh;

	mesh.Load(
		//ここに3Dモデルを読みこみ 今仮表示あとで四角のやつ作ってね
		"assets/model/Enemy/teki.fbx",
		"assets/model/Enemy"
	);

	m_MeshRenderer.Init(mesh);

	m_Shader.Create(
		"shader/litTextureVS.hlsl",
		"shader/enemyFlash.hlsl"
	);
	m_Shader.CreatePSConstantBuffer(sizeof(FlashParams));

	m_Subsets = mesh.GetSubsets();
	m_Textures = mesh.GetTextures();

	auto materials = mesh.GetMaterials();

	for (auto& m : materials)
	{
		auto material = std::make_unique<Material>();
		material->Create(m);
		m_Materials.emplace_back(std::move(material));
	}
}

void Enemy::Update()
{
	const float dt = 1.0f / 60.0f;

	//回転：瀕死・倒れた後はゆっくり
	m_Rotation.y += (m_State == State::Alive) ? 0.01f : 0.003f;

	if (m_HitTimer > 0.0f) m_HitTimer -= dt;
	if (m_FlashTime > 0.0f) m_FlashTime = std::max(0.0f, m_FlashTime - dt);

	//瀕死0.6sごとに赤く点滅
	if (m_State == State::Dying)
	{
		m_DyingBlink -= dt;
		if (m_DyingBlink <= 0.0f)
		{
			Flash(Vector3(1.0f, 0.2f, 0.15f), 0.5f, 0.25f);
			m_DyingBlink = 0.6f;
		}
	}

	//とどめのあと白くなって爆発
	if (m_State == State::Defeated && !m_Exploded)
	{
		m_DefeatTime += dt;
		float k = std::min(m_DefeatTime / kExplodeTime, 1.0f);
		Flash(Vector3(1.0f, 1.0f, 1.0f), 0.3f + 0.7f * k, 0.05f);	//ちょっとずつ白く
		if (m_DefeatTime >= kExplodeTime)
		{
			m_Exploded = true;
			if (m_OnExploded) m_OnExploded();
		}
	}

}

void Enemy::Draw(Camera* cam)
{
	if (m_Exploded) return;
	cam->SetCamera(0);

	Renderer::SetBlendState(BS_NONE);

	//深度を書き込まない
	Renderer::SetDepthEnable(false);

	m_Shader.SetGPU();

	//今の光り具合をシェーダーへ送る
	FlashParams fp;
	fp.color = DirectX::SimpleMath::Vector4(m_FlashColor.x, m_FlashColor.y, m_FlashColor.z, CurrentFlash());
	m_Shader.UpdatePSConstantBuffer(&fp, sizeof(fp));

	m_MeshRenderer.BeforeDraw();

	Matrix s = Matrix::CreateScale(m_Size);
	//攻撃を受けた直後は左右に細かく揺らす
	float k = std::max(0.0f, m_HitTimer / 0.25f);
	Vector3 shake(sinf(m_HitTimer * 120.0f) * 12.0f * k, 0.0f, 0.0f);
	if (m_State == State::Defeated)
	{
		float amp = 6.0f + 30.0f * std::min(m_DefeatTime / kExplodeTime, 1.0f);
		shake = Vector3(sinf(m_DefeatTime * 90.0f) * amp, cosf(m_DefeatTime * 70.0f) * amp * 0.5f, 0.0f);
	}
	Matrix t = Matrix::CreateTranslation(m_Position + shake);
	Matrix x = Matrix::CreateRotationX(m_Rotation.x);
	Matrix y = Matrix::CreateRotationY(m_Rotation.y);
	Matrix z = Matrix::CreateRotationZ(m_Rotation.z);

	Matrix world = s * x * y * z * t;

	Renderer::SetWorldMatrix(&world);

	for (size_t i = 0; i < m_Subsets.size(); i++)
	{
		m_Materials[m_Subsets[i].MaterialIdx]->SetGPU();

		if (m_Materials[m_Subsets[i].MaterialIdx]->isTextureEnable())
		{
			m_Textures[m_Subsets[i].MaterialIdx]->SetGPU();
		}
		m_MeshRenderer.DrawSubset(
			m_Subsets[i].IndexNum,
			m_Subsets[i].IndexBase,
			m_Subsets[i].VertexBase
		);
	}
	Renderer::SetDepthEnable(true);
}

void Enemy::Uninit()
{
}

void Enemy::Finish()
{
	if (m_State == State::Defeated) return;	//2回目はなにもしない
	m_State = State::Defeated;
	m_Hp = 0.0f;
	m_DefeatTime = 0.0f;
	Flash(Vector3(1.0f, 1.0f, 1.0f), 1.0f, 0.15f);	//とどめの瞬間真っ白
	if (m_OnDefeated) m_OnDefeated();
}

void Enemy::Flash(const DirectX::SimpleMath::Vector3& color, float strength, float duration)
{
	//今光っている方が強ければ、そちらを残す（弱い光で強い光を上書きしない）
	if (strength < CurrentFlash()) return;

	m_FlashColor = color;
	m_FlashStrength = strength;
	m_FlashDuration = std::max(0.01f, duration);
	m_FlashTime = m_FlashDuration;
}

//今の光り具合：光った瞬間が一番強く、2乗で素早く元に戻る
float Enemy::CurrentFlash() const
{
	if (m_FlashTime <= 0.0f) return 0.0f;
	float k = m_FlashTime / m_FlashDuration;	// 1->0
	return m_FlashStrength * k * k;
}
