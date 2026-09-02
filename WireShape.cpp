#include "WireShape.h"
#include "Game.h"
#include "Camera.h"

using namespace DirectX::SimpleMath;

void WireShape::SetUp(float x, float y, float z, float w, float h, DirectX::SimpleMath::Color col, float speed)
{
    m_X = x;
    m_Y = y;
    m_Z = z;
    m_Width = w;
    m_Height = h;
    m_Color = col;
    m_ScrollSpeed = speed;

    if (m_pVertexBuffer)
    {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = nullptr;
    }
 
    float hw = m_Width * 0.5f;
    float hh = m_Height * 0.5f;

    std::vector<LineVertex> verts;
    //ã•Ó
    verts.push_back({ Vector3(-hw,hh,0),Vector3(0,1,0),m_Color, Vector2(0,0) });
    verts.push_back({ Vector3(hw,hh,0),Vector3(0,1,0),m_Color,Vector2(0,0) });
    //‰º•Ó
    verts.push_back({ Vector3(-hw,-hh,0),Vector3(0,1,0),m_Color, Vector2(0,0) });
    verts.push_back({ Vector3(hw,-hh,0),Vector3(0,1,0),m_Color,Vector2(0,0) });
    //¶•Ó
    verts.push_back({ Vector3(-hw,hh,0),Vector3(0,1,0),m_Color, Vector2(0,0) });
    verts.push_back({ Vector3(-hw,-hh,0),Vector3(0,1,0),m_Color,Vector2(0,0) });
    //‰E•Ó
    verts.push_back({ Vector3(hw,hh,0),Vector3(0,1,0),m_Color, Vector2(0,0) });
    verts.push_back({ Vector3(hw,-hh,0),Vector3(0,1,0),m_Color,Vector2(0,0) });

    m_VertexCount = (int)verts.size();

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(LineVertex) * m_VertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA id{};
    id.pSysMem = verts.data();

    Renderer::GetDevice()->CreateBuffer(&bd, &id, &m_pVertexBuffer);
}

void WireShape::Init()
{
    m_Shader.Create("shader/wireGridVS.hlsl", "shader/wireGridPS.hlsl");
}

void WireShape::Update()
{
    m_Z -= m_ScrollSpeed * Game::GetInstance()->GetDeltaTime();
}

void WireShape::Draw(Camera* cam)
{
    Renderer::SetDepthEnable(true);
    Renderer::SetBlendState(BS_ALPHABLEND);

    cam->SetCamera(1);

    Matrix r = Matrix::CreateFromYawPitchRoll(m_RotX, m_RotY, m_RotZ);
    Matrix t = Matrix::CreateTranslation(m_X, m_Y, m_Z);
    Matrix world = r * t;
    Renderer::SetWorldMatrix(&world);

    auto* dc = Renderer::GetDeviceContext();
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    m_Shader.SetGPU();

    UINT stride = sizeof(LineVertex);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

    dc->Draw(m_VertexCount, 0);
}

void WireShape::Uninit()
{
    if (m_pVertexBuffer) {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = nullptr;
    }
}

