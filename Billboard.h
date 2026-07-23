#ifndef __BILLBOARD_H__
#define __BILLBOARD_H__

#include <DirectXMath.h>
#include <memory>
#include "MeshBuffer.h"
#include "Shader.h"
#include "Texture.h"

class Billboard
{
public:
	static void Init();
	static void Uninit();
	static void Draw();

	static void SetWorld(DirectX::XMFLOAT4X4 world);
	static void SetView(DirectX::XMFLOAT4X4 view);
	static void SetProjection(DirectX::XMFLOAT4X4 proj);
	static void SetColor(DirectX::XMFLOAT4 color);
	static void SetTexture(Texture* tex);
	static void SetBlendAdd(bool enable);	// true:加算合成 false:通常のアルファブレンド

private:
	struct Data
	{
		std::shared_ptr<VertexShader> defVS;
		std::shared_ptr<PixelShader> defPS;
		std::shared_ptr<MeshBuffer> mesh;
		DirectX::XMFLOAT4X4 matrix[3];
		DirectX::XMFLOAT4 color;
		Texture* texture;
		bool blendAdd;
	};
	static Data m_data;
	static std::shared_ptr<Texture> m_whiteTex;
};

#endif // __BILLBOARD_H__
