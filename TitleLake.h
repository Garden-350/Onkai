#pragma once
//空・雲・太陽・月・星とうを描く　lakePS.hlslを使用してるよ
//あと風が吹くのとかもここが担当

#include "Object.h"
#include "LakeParms.h"
#include "LakeGrass.h"
#include "LakeParticles.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <chrono>

class TitleLake : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override {}

	//Enterを押した時の飛び込みのやつ
	void StartDive();
	bool IsDiving() const { return m_Diving; }
	bool IsDiveFinished() const { return m_DiveFinished; }
	float GetDiveProgress() const;							// 0～1
	float GetDiveFade() const;								// 水に入った瞬間に画面を覆う濃さ 0～1
	DirectX::SimpleMath::Vector3 GetDiveFadeColor() const;	// そのときの色（昼は明るい水色、夜は暗い青）

	float GetIntroFade() const;								// 起動時の黒フェードの濃さ 1→0
	float GetNightAmount() const { return m_Params.sunColor.w; }	// 0=昼 1=夜（ロゴの色を変えるのに使う）

private:
	void UpdateTimeOfDay();
	void UpdateWind(float dt);
	void UpdateCamera();

	LakeParms m_Params = {};
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_LakeCB;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_FullVS;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_FullLayout;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_FullPS;

	LakeGrass m_Grass;
	LakeParticles m_Particles;

	std::chrono::steady_clock::time_point m_LastClock;
	float m_Time = 0.0f;
	float m_Dt = 0.0f;

	//時間帯
	float m_DayLength = 60.0f;	//1日が何秒か
	float m_StartPhase = 0.27f;	//タイトルが始まった時の時刻　０＝真夜中とか

	//風
	DirectX::SimpleMath::Vector2 m_WindDir = DirectX::SimpleMath::Vector2(0.87f, 0.5f);	//風が吹いていく向き
	float m_WindBase = 0.3f;	//ふだんの風の強さ
	float m_GustMinInterval = 6.0f;	//突風の間隔　短い方
	float m_GustMaxInterval = 14.0f;//突風の間隔　長い方
	float m_GustSeconds = 4.5f;	//突風一回の長さ
	float m_NextGust = 4.0f;		//次の突風まで
	float m_GustTimer = -1.0f;		//突風が始まってからの秒数
	float m_GustPeak = 1.0f;
	float m_Gust = 0.0f;			//今の突風の強さを01で
	float m_GustFront = -1000000.0f;	//突風の先頭の位置

	//カメラ
	float m_ShoreBaseZ = 450.0f;	//手前の岸までの距離（水面が始まるところ）
	DirectX::SimpleMath::Vector3 m_EyePos = DirectX::SimpleMath::Vector3(0.0f, 150.0f, 0.0f);	//目の高さ150.0f(草の上から見降ろす感じ)
	float m_PitchDeg = -3.0f;	//上下の向き（マイナスで見降ろし）
	float m_Exposure = 0.75f;	//全体の明るさ
	DirectX::SimpleMath::Vector3 m_CamPos;
	DirectX::SimpleMath::Vector3 m_CamTarget;

	//飛び込み
	bool m_Diving = false;
	bool m_DiveContact = false;	//水面に着いた
	bool m_DiveFinished = false;
	float m_DiveTimer = 0.0f;
	float m_ContactTimer = 0.0f;
	float m_DiveSeconds = 1.8f;	//Enterを押してから水面に着くまで
	float m_DiveHoldSeconds = 0.3f;	//水に入ってから切替までの時間
	float m_DivePitchDeg = -38.0f;	//飛び込むときの見降ろす角度
	DirectX::SimpleMath::Vector3 m_DiveStart;
};

