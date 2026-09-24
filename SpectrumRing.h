#pragma once
// ============================================================
// SpectrumRing : 曲選択画面で、中央のジャケットを囲むように
//   円形のオーディオスペクトラム（放射状のバー）を描くオブジェクト。
//
//   ・解析(FFT)はしない。SelectScene が AudioSpectrum で求めた
//     帯域ごとの値(0〜1)を SetLevels で毎フレーム渡す。
//   ・左右対称：真上が低音、真下に向かって高音。右半分と左半分が鏡写し。
//   ・2Dカメラ(Texture2Dと同じ座標系：画面中心が原点、Y上向き)で描く。
//   ・テクスチャ不要（形と光り方は spectrumBarPS.hlsl で計算する）。
// ============================================================
#include "Object.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include <SimpleMath.h>
#include <vector>

class SpectrumRing : public Object
{
public:
	void Init() override;
	void Update() override {}
	void Draw(Camera* cam) override;
	void Uninit() override {}

	// levels : 帯域ごとの強さ(0〜1)。要素数 = 片側のバー本数
	// dt     : 前フレームからの経過秒（なめらかに追従させるのに使う）
	void SetLevels(const std::vector<float>& levels, float dt);

	// 低音の強さ(0〜1)。ジャケットを鼓動させる等に使える
	float GetBass() const { return m_Bass; }

	// 中心位置（Texture2Dと同じ2D座標。zはジャケットより奥・背景より手前にする）
	void SetCenter(const DirectX::SimpleMath::Vector3& center) { m_Center = center; }
	void SetRadius(float radius) { m_Radius = radius; }

private:
	struct SpectrumParams
	{
		DirectX::SimpleMath::Vector4 color;	// rgb=色, a=不透明度
		float mode;			// 0=バー, 1=土台の円, 2=背景を暗くする円
		float ringWidth;	// mode1用：円の線の太さ(UV単位)
		float padding[2];	// 16バイト境界合わせ
	};	// sizeof == 32

	void DrawQuad(const DirectX::SimpleMath::Matrix& world, const SpectrumParams& params);

	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;

	std::vector<float> m_Levels;	// なめらかにした表示用の値
	float m_Bass = 0.0f;
	float m_Spin = 0.0f;			// 全体をゆっくり回す角度(度)

	// ── 見た目の調整用 ──
	DirectX::SimpleMath::Vector3 m_Center = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 450.0f);
	float m_Radius = 190.0f;		// バーの根本の円の半径（中央ジャケット250pxの角(約177)より少し外）
	float m_BarWidth = 9.0f;		// バー1本の太さ
	float m_MinLength = 6.0f;		// 無音のときのバーの長さ
	float m_MaxLength = 130.0f;		// 最大音量のときに伸びる長さ
	float m_SpinSpeed = 4.0f;		// 全体の回転速度(度/秒)。0で回転しない
	float m_BackingAlpha = 0.40f;	// 輪の後ろを暗くする濃さ(0で暗くしない)。背景が明るいとき用
};
