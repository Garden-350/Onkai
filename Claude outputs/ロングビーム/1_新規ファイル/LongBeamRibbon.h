#pragma once
#include <vector>
#include <SimpleMath.h>
#include "Renderer.h"
#include "VertexBuffer.h"
#include "Shader.h"
#include "Texture.h"

class Camera;

// ============================================================
// LongBeamRibbon : 点の列をつないだ「1本の光の帯」を描くクラス。
//   点ごとにカメラの方を向いた左右2つの頂点を作り、三角形ストリップで
//   1回の描画でつなげて描く。UVのvを線に沿って連続させるので、
//   短い四角を並べたときのような「つなぎ目」が出ない。
// ============================================================
class LongBeamRibbon
{
public:
	struct Point
	{
		DirectX::SimpleMath::Vector3 pos;	// 帯の中心
		DirectX::SimpleMath::Color   color;	// その点の芯の色（rgb）と濃さ（a。奥でフェードさせるのに使う）
	};

	void Init();

	// points    : 手前（判定ライン側）から奥へ順に並べた点
	// width     : 帯の幅（ワールド座標）
	// intensity : 明るさ（押せていない時は下げる）
	// time      : 模様を流すための時間（曲時間を渡す）
	// glowColor : 外側の柔らかい光の色
	void Draw(Camera* cam, const std::vector<Point>& points, float width, float intensity,
		float time, const DirectX::SimpleMath::Vector3& glowColor);

private:
	static constexpr int kMaxPoints = 512;	// 1本あたりの点の上限

	struct Params
	{
		DirectX::SimpleMath::Vector4 glowColor;
		float intensity;
		float time;
		float padding[2];
	};	// 32バイト（16の倍数）

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	std::vector<VERTEX_3D>  m_Vertices;
	Shader  m_Shader;
	Texture m_Noise;
	bool    m_NoiseLoaded = false;
};
