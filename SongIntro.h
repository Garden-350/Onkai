#pragma once
#include "Object.h"
#include "Texture2D.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include <chrono>
#include <string>

class SongIntro : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }	//動いてはだめだから

	void SetSong(const std::string& name);	//譜面の名前から探索
	void SetLeaveTime(float seconds) { m_LeaveTime = seconds; }	//消え始める時刻

private:
	//画面のcx,cyを中心に幅、高さの四角をcolorでぬる
	void DrawRect(float cx, float cy, float w, float h, const DirectX::SimpleMath::Vector4& color);
	static float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
	static float EaseOut(float t) { t = Clamp01(t); float u = 1.0f - t; return 1.0f - u * u * u; }	//最初速く、最後ゆっくり

	Texture2D m_Jacket;
	Texture2D m_Name;
	VertexBuffer<VERTEX_3D> m_RectVB;
	IndexBuffer m_RectIB;

	float m_Time = 0.0f;	//シーンが始まってからの秒数
	float m_LeaveTime = 1.5f;	//消え始める時間
	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	//調整用
	static constexpr float kOpenTime = 0.3f;	//帯が開いていくまでの時間
	static constexpr float kSlideTime = 0.35f;	//ジャケット・曲名が入ってくるまでの秒数
	static constexpr float kNameDelay = 0.1f;	//曲名をジャケットよりちょっとだけ遅らせる
	static constexpr float kLeaveDuration = 0.35f;	//消えるまでの秒数
	static constexpr float kCenterY = 40.0f;		//帯の真ん中の高さ
	static constexpr float kBandH = 280.0f;			//帯の太さ
	static constexpr float kBandAlpha = 0.6f;		//濃さ
	static constexpr float kJacketSize = 200.0f;
	static constexpr float kJacketX = -360.0f;		//ジャケットの横の位置
	static constexpr float kNameW = 600.0f;			//曲目の画像幅　画像は3:1
	static constexpr float kNameH = 200.0f;
	static constexpr float kNameX = 110.0f;			//曲名の横の位置 文字の左端がジャケットのすぐ右に来る
	static constexpr float kSlideDist = 700.0f;		//どれだけ遠くから入ってくるか
	static constexpr float kLeaveRise = 40.0f;		//消える時に上へずれる量
};

