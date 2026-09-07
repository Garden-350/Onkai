# ComboHud 切り出しガイド

ChartObject.cpp/.h から「コンボ数字表示・判定ランクのポップ演出・PERFECT/GREAT/GOOD/MISS内訳表示・障壁(バリア)演出」を ComboHud という新しいクラスに切り出す手順です。JudgeSystemのときと同じ形式で、削除箇所・追加箇所を書いています。

## 設計方針

- トリガー（`ChartObject::OnJudge`）・減衰（`Update`）・描画（`Draw`）の3箇所に分散していたコンボ/判定ランク/障壁まわりのコードを、ComboHudの `TriggerXxx()` / `Update()` / `Draw()` に1つずつ対応させてまとめる。
- 判定ランクのポップ演出は、判定円をスクリーン座標に変換した位置(`WorldToTexture2DPos`の結果)にY+150して表示する。障壁演出は判定円の3Dワールド座標をそのまま使う。どちらもChartObject::Draw()側で使っている既存の計算をそのまま再利用できるよう、`Draw()`の判定円ループ内で配列にキャッシュしてComboHudへ渡す形にした（`WorldToTexture2DPos`をComboHud.cpp側に複製する必要がない）。
- レーンカラー(`GetLaneColor`)は今まで通りChartObjectの静的関数として残し、ComboHud呼び出し直前に6レーン分をまとめて計算して配列で渡す。
- パーティクル(m_Particle)とライトエフェクト(m_LightEffects)は今回のスコープには含めない。これらはOnJudge/DrawLightEffects側にそのまま残る。

---

## 1. 新規ファイル: ComboHud.h

```cpp
#pragma once
#include "Texture2D.h"
#include "BarrierQuad.h"
#include <SimpleMath.h>

class Camera;

struct BarrierEffect
{
	bool active = false;
	float timer = 0.0f;
};

// ============================================================
// ComboHud : コンボ数字・判定内訳(PERFECT/GREAT/GOOD/MISS)・
//   判定ランクのポップ演出・障壁演出(バリア)をまとめて担当するクラス。
//   トリガー(TriggerXxx)はChartObject::OnJudgeから、
//   減衰(Update)は毎フレーム、描画(Draw)はChartObject::Drawの後半から呼ぶ。
// ============================================================
class ComboHud
{
public:
	void Init();

	void TriggerRankPop(int laneIndex, int rank); // rank: 0=PERFECT,1=GREAT,2=GOOD
	void TriggerBarrier(int laneIndex);
	void TriggerComboPop();

	void Update(float deltaTime);

	// judgeCircleScreenPos/judgeCirclePos3D/laneColors はいずれも
	// ChartObject::Draw側で判定円ループの中で計算済みの値を渡してもらう（6レーン分）
	void Draw(Camera* cam, const DirectX::SimpleMath::Matrix& view,
		int combo, int perfect, int great, int good, int miss,
		const DirectX::SimpleMath::Vector3 judgeCircleScreenPos[6],
		const DirectX::SimpleMath::Vector3 judgeCirclePos3D[6],
		const DirectX::SimpleMath::Vector3 laneColors[6]);

private:
	struct JudgeRankEffect
	{
		bool active = false;
		float timer = 0.0f;
		int rank = 0; // 0=PERFECT, 1=GREAT, 2=GOOD
	};

	Texture2D m_ComboDigits[4];	// コンボ数の3桁表示
	Texture2D m_PerfectDigits[4];
	Texture2D m_GreatDigits[4];
	Texture2D m_GoodDigits[4];
	Texture2D m_MissDigits[4];

	Texture2D m_JudgeRankPerfect;
	Texture2D m_JudgeRankGreat;
	Texture2D m_JudgeRankGood;
	JudgeRankEffect m_JudgeRankEffects[6];
	static constexpr float kJudgeRankPopDuration = 0.6f; // 表示継続時間（秒）

	BarrierEffect m_BarrierEffects[6];
	BarrierQuad m_BarrierQuad;
	static constexpr float kBarrierDuration = 10.25f;

	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
};
```

---

## 2. 新規ファイル: ComboHud.cpp

```cpp
#include "ComboHud.h"
#include "Renderer.h"
#include "Camera.h"

using namespace DirectX::SimpleMath;

void ComboHud::Init()
{
	// ── コンボ数字(4桁分)のセットアップ ──
	for (int i = 0; i < 4; i++)
	{
		m_ComboDigits[i].Init();
		m_ComboDigits[i].SetTexture("assets/texture/allnumber_new.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(m_NuberSplite_Size, m_NuberSplite_Size, 1.0f);
	}

	auto initDigitRow = [this](Texture2D* row)
		{
			for (int i = 0; i < 4; i++)
			{
				row[i].Init();
				row[i].SetTexture("assets/texture/allnumber_new.png");
				row[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
				row[i].SetScale(m_NuberSplite_Size * 0.6f, m_NuberSplite_Size * 0.6f, 1.0f); // コンボより小さめ
			}
		};
	initDigitRow(m_PerfectDigits);
	initDigitRow(m_GreatDigits);
	initDigitRow(m_GoodDigits);
	initDigitRow(m_MissDigits);

	m_JudgeRankPerfect.Init();
	m_JudgeRankPerfect.SetTexture("assets/texture/PERFECT.png");
	m_JudgeRankPerfect.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGreat.Init();
	m_JudgeRankGreat.SetTexture("assets/texture/GREAT.png");
	m_JudgeRankGreat.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGood.Init();
	m_JudgeRankGood.SetTexture("assets/texture/GOOD.png");
	m_JudgeRankGood.SetScale(200.0f, 50.0f, 1.0f);

	m_BarrierQuad.Init();
}

void ComboHud::TriggerRankPop(int laneIndex, int rank)
{
	if (laneIndex < 0 || laneIndex >= 6) return;
	m_JudgeRankEffects[laneIndex].active = true;
	m_JudgeRankEffects[laneIndex].rank = rank;
	m_JudgeRankEffects[laneIndex].timer = kJudgeRankPopDuration;
}

void ComboHud::TriggerBarrier(int laneIndex)
{
	if (laneIndex < 0 || laneIndex >= 6) return;
	m_BarrierEffects[laneIndex].active = true;
	m_BarrierEffects[laneIndex].timer = 0.0f;
}

void ComboHud::TriggerComboPop()
{
	m_ComboPopTimer = 0.15f; // コンボ数字が一瞬拡大表示される演出用タイマー
}

void ComboHud::Update(float deltaTime)
{
	// コンボ数字のポップアニメーションのタイマー減算
	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

	//ボタン入力時に出る障壁
	//for (int i = 0; i < 6; i++)
	//{
	//	if (!m_BarrierEffects[i].active) continue;
	//	m_BarrierEffects[i].timer += deltaTime;
	//	if (m_BarrierEffects[i].timer >= kBarrierDuration)
	//		m_BarrierEffects[i].active = false;
	//}

	for (int i = 0; i < 6; i++)
	{
		if (!m_JudgeRankEffects[i].active) continue;
		m_JudgeRankEffects[i].timer -= deltaTime;
		if (m_JudgeRankEffects[i].timer <= 0.0f)
		{
			m_JudgeRankEffects[i].timer = 0.0f;
			m_JudgeRankEffects[i].active = false;
		}
	}
}

void ComboHud::Draw(Camera* cam, const Matrix& view,
	int combo, int perfect, int great, int good, int miss,
	const Vector3 judgeCircleScreenPos[6],
	const Vector3 judgeCirclePos3D[6],
	const Vector3 laneColors[6])
{
	//判定ランク演出
	{
		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 6; i++)
		{
			if (!m_JudgeRankEffects[i].active) continue;

			Texture2D* target = nullptr;
			switch (m_JudgeRankEffects[i].rank)
			{
			case 0: target = &m_JudgeRankPerfect; break;
			case 1: target = &m_JudgeRankGreat;   break;
			case 2: target = &m_JudgeRankGood;    break;
			}
			if (!target) continue;

			Vector3 screenPos = judgeCircleScreenPos[i];
			screenPos.y += 150.0f;

			target->SetPosition(screenPos);
			target->Draw(cam);
		}
		Renderer::SetBlendState(BS_NONE);
	}

	// ── コンボ数表示（3桁、ヒット直後は少し拡大する演出）──
	{
		int comboClamped = combo;
		if (comboClamped > 999) comboClamped = 999;

		int digits[3] = { comboClamped / 100,(comboClamped / 10) % 10, comboClamped % 10 };
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 40.0f;
		float startX = 200.0f;

		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 3; i++)
		{
			m_ComboDigits[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
			m_ComboDigits[i].SetScale(m_NuberSplite_Size * popScale, m_NuberSplite_Size * popScale, 1.0f);
			m_ComboDigits[i].SetPosition(startX + digitW * i, 250.0f, 0.0f);
			m_ComboDigits[i].Draw(cam);
		}
	}

	// ── 判定内訳の表示（PERFECT/GREAT/GOOD/MISS）──
	{
		auto drawCountRow = [&](Texture2D* row, int value, float y)
			{
				if (value > 9999) value = 9999;
				int digits[4] = { value / 1000, value / 100, (value / 10) % 10, value % 10 };
				float digitW = 26.0f;   // コンボより詰めて表示
				float startX = 40.0f;

				for (int i = 0; i < 4; i++)
				{
					row[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
					row[i].SetPosition(startX + digitW * i, y - 100.0f, 0.0f);
					row[i].Draw(cam);
				}
			};
		float topY = 400.0f;	//一番上のY座標
		float rowSpacing = 80.0f;	//行間

		Renderer::SetBlendState(BS_ALPHABLEND);
		drawCountRow(m_PerfectDigits, perfect, topY);
		drawCountRow(m_GreatDigits, great, topY - rowSpacing);
		drawCountRow(m_GoodDigits, good, topY - rowSpacing * 2);
		drawCountRow(m_MissDigits, miss, topY - rowSpacing * 3);
		Renderer::SetBlendState(BS_NONE);
	}

	Renderer::SetBlendState(BS_ADDITIVE);
	for (int i = 0; i < 6; i++)
	{
		if (!m_BarrierEffects[i].active) continue;
		float progress = m_BarrierEffects[i].timer / kBarrierDuration;
		m_BarrierQuad.Draw(judgeCirclePos3D[i], 300.0f, progress, laneColors[i], view);
	}
	Renderer::SetBlendState(BS_NONE);
}
```

---

## 3. ChartObject.h の変更点

### 3-1. include追加

`#include "JudgeSystem.h"` の下に1行追加：

```cpp
#include "JudgeSystem.h"
#include "ComboHud.h"
```

### 3-2. グローバルの `struct BarrierEffect` を削除

削除（`ChartHomingBeam / BeamShaderParms は...` のコメントの少し下、`ChartKeyLightEffect` 構造体の上にあります）:

```cpp
struct BarrierEffect
{
	bool active = false;
	float timer = 0.0f;
};

```

（ComboHud.h に同じものが入っています。`struct ChartKeyLightEffect` はそのまま残してください。）

### 3-3. m_BarrierQuad を削除

削除:
```cpp
	BarrierQuad m_BarrierQuad;
```

### 3-4. コンボ/判定内訳の数字テクスチャ配列を削除

削除:
```cpp
	Texture2D m_ComboDigits[4];	// コンボ数の3桁表示
	Texture2D m_PerfectDigits[4];
	Texture2D m_GreatDigits[4];
	Texture2D m_GoodDigits[4];
	Texture2D m_MissDigits[4];
```

（この上下にある `Texture2D m_LaneIcons[6];` や `Texture2D m_LaneFrame[6];` は消さないでください。あくまでComboDigits〜MissDigitsの5行だけです。）

### 3-5. m_BarrierEffects を削除

削除:
```cpp
	BarrierEffect m_BarrierEffects[6];
```

### 3-6. m_ComboPopTimer / m_NuberSplite_Size を削除

削除:
```cpp
	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
```

（同じ場所にある `float m_FrameSize = 150.0f;` は判定円ガイド枠のサイズなので**残してください**。）

### 3-7. JudgeRankEffect構造体〜kJudgeRankPopDurationまでを削除

削除:
```cpp
	struct JudgeRankEffect
	{
		bool active = false;
		float timer = 0.0f;
		int rank = 0; // 0=PERFECT, 1=GREAT, 2=GOOD
	};

	Texture2D m_JudgeRankPerfect;
	Texture2D m_JudgeRankGreat;
	Texture2D m_JudgeRankGood;
	JudgeRankEffect m_JudgeRankEffects[6];
	static constexpr float kJudgeRankPopDuration = 0.6f; // 表示継続時間（秒）
```

（この下にある `static DirectX::SimpleMath::Vector3 GetLaneColor(int lane) {...}` は**そのまま残してください**。ComboHudからは使わず、ChartObject側で完結させます。）

### 3-8. kBarrierDuration を削除し、ComboHudメンバを追加

ファイル末尾、クラスの最後の部分です。

削除:
```cpp
	//ボタンを押したときに出すエフェクトの一種
	static constexpr float kBarrierDuration = 10.25f;
```

追加（同じ場所に）:
```cpp
	ComboHud m_ComboHud;
```

以上でChartObject.hは完了です。

---

## 4. ChartObject.cpp の変更点

### 4-1. Init() 内、コンボ数字のセットアップを削除

削除:
```cpp
	// ── コンボ数字(4桁分)のセットアップ ──
	for (int i = 0; i < 4; i++)
	{
		m_ComboDigits[i].Init();
		m_ComboDigits[i].SetTexture("assets/texture/allnumber_new.png");
		m_ComboDigits[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f); // 初期表示は "0"
		m_ComboDigits[i].SetScale(m_NuberSplite_Size, m_NuberSplite_Size, 1.0f);
	}

	auto initDigitRow = [this](Texture2D* row)
		{
			for (int i = 0; i < 4; i++)
			{
				row[i].Init();
				row[i].SetTexture("assets/texture/allnumber_new.png");
				row[i].SetUV(1.0f, 1.0f, 10.0f, 1.0f);
				row[i].SetScale(m_NuberSplite_Size * 0.6f, m_NuberSplite_Size * 0.6f, 1.0f); // コンボより小さめ
			}
		};
	initDigitRow(m_PerfectDigits);
	initDigitRow(m_GreatDigits);
	initDigitRow(m_GoodDigits);
	initDigitRow(m_MissDigits);

```

（この後に続く `// ── タイミングガイド：外枠と育つ四角のセットアップ ──` から始まるレーン枠のセットアップは**そのまま残してください**。今回消すのはコンボ数字部分だけです。）

何も追加しなくてOKです（ComboHud.Init()呼び出しは次の項目でまとめて行います）。

### 4-2. Init() 内、判定ランクテクスチャ＋バリア初期化を1行にまとめる

削除:
```cpp
	m_JudgeRankPerfect.Init();
	m_JudgeRankPerfect.SetTexture("assets/texture/PERFECT.png");
	m_JudgeRankPerfect.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGreat.Init();
	m_JudgeRankGreat.SetTexture("assets/texture/GREAT.png");
	m_JudgeRankGreat.SetScale(200.0f, 50.0f, 1.0f);

	m_JudgeRankGood.Init();
	m_JudgeRankGood.SetTexture("assets/texture/GOOD.png");
	m_JudgeRankGood.SetScale(200.0f, 50.0f, 1.0f);

	m_BarrierQuad.Init();
```

追加:
```cpp
	m_ComboHud.Init();
```

### 4-3. Update() 内、コンボポップタイマー減算を削除

削除:
```cpp
	// コンボ数字のポップアニメーションのタイマー減算
	if (m_ComboPopTimer > 0.0f)
	{
		m_ComboPopTimer -= deltaTime;
		if (m_ComboPopTimer < 0.0f) m_ComboPopTimer = 0.0f;
	}

```

何も追加しなくてOKです（この処理はComboHud::Updateに移り、後述4-4でまとめて呼びます）。この後に続く「判定円の動きに合わせて〜」のノーツ追従ループはそのまま残してください。

### 4-4. Update() 内、障壁コメント＋判定ランク減衰ループを ComboHud::Update呼び出しに置き換え

削除:
```cpp
	//ボタン入力時に出る障壁
	//for (int i = 0; i < 6; i++)
	//{
	//	if (!m_BarrierEffects[i].active) continue;
	//	m_BarrierEffects[i].timer += deltaTime;
	//	if (m_BarrierEffects[i].timer >= kBarrierDuration)
	//		m_BarrierEffects[i].active = false;
	//}

	for (int i = 0; i < 6; i++)
	{
		if (!m_JudgeRankEffects[i].active) continue;
		m_JudgeRankEffects[i].timer -= deltaTime;
		if (m_JudgeRankEffects[i].timer <= 0.0f)
		{
			m_JudgeRankEffects[i].timer = 0.0f;
			m_JudgeRankEffects[i].active = false;
		}
	}
```

追加:
```cpp
	m_ComboHud.Update(deltaTime);
```

### 4-5. OnJudge() 内、トリガー呼び出しに置き換え

削除:
```cpp
	if (laneIndex >= 0 && laneIndex < 6)
	{
		m_BarrierEffects[laneIndex].active = true;
		m_BarrierEffects[laneIndex].timer = 0.0f;

		m_JudgeRankEffects[laneIndex].active = true;
		m_JudgeRankEffects[laneIndex].rank = rank;
		m_JudgeRankEffects[laneIndex].timer = kJudgeRankPopDuration;
	}

	m_ComboPopTimer = 0.15f; // コンボ数字が一瞬拡大表示される演出用タイマー
```

追加:
```cpp
	m_ComboHud.TriggerBarrier(laneIndex);
	m_ComboHud.TriggerRankPop(laneIndex, rank);
	m_ComboHud.TriggerComboPop();
```

（境界チェックは各Trigger関数の内部でやっているので、ここで`if (laneIndex >= 0 && laneIndex < 6)`を書く必要はありません。）

### 4-6. Draw() 内、判定円ループで座標をキャッシュする

`// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──` のすぐ下、ループの直前と、ループの中身の両方を変更します。

削除:
```cpp
	// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		Vector3 circlePos3D = m_JudgeCircles[i].GetPosition(m_MusicTime);
		Vector3 screenPos = WorldToTexture2DPos(circlePos3D, view);
```

追加:
```cpp
	// ── 判定円：judgeCirclesの3D座標を画面に投影して表示 ──
	// （ここで計算した座標は、後段のComboHud::Draw呼び出しでも再利用する）
	Vector3 judgeCirclePos3D[6];
	Vector3 judgeCircleScreenPos[6];

	Renderer::SetBlendState(BS_ALPHABLEND);
	for (int i = 0; i < 6; i++)
	{
		Vector3 circlePos3D = m_JudgeCircles[i].GetPosition(m_MusicTime);
		Vector3 screenPos = WorldToTexture2DPos(circlePos3D, view);
		judgeCirclePos3D[i] = circlePos3D;
		judgeCircleScreenPos[i] = screenPos;
```

（ループの中身はこの2行を追加するだけで、それ以降の `if (m_ShowLaneKeyIcons)` 等はそのまま変更しません。）

### 4-7. Draw() 内、判定ランク演出〜障壁演出のブロックをまるごと置き換え

`//判定ランク演出` から始まり、障壁演出の `Renderer::SetBlendState(BS_NONE);`（`DrawLightEffects(cam);` の直前）までをまるごと削除します。

削除:
```cpp
	//判定ランク演出
	{
		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 6; i++)
		{
			if (!m_JudgeRankEffects[i].active) continue;

			Texture2D* target = nullptr;
			switch (m_JudgeRankEffects[i].rank)
			{
			case 0: target = &m_JudgeRankPerfect; break;
			case 1: target = &m_JudgeRankGreat;   break;
			case 2: target = &m_JudgeRankGood;    break;
			}
			if (!target) continue;

			Vector3 circlePos3D = m_JudgeCircles[i].GetPosition(m_MusicTime);
			Vector3 screenPos = WorldToTexture2DPos(circlePos3D, view);
			screenPos.y += 150.0f;

			target->SetPosition(screenPos);
			target->Draw(cam);
		}
		Renderer::SetBlendState(BS_NONE);
	}

	// ── コンボ数表示（3桁、ヒット直後は少し拡大する演出）──
	{
		int combo = m_JudgeSystem.GetComboCount();
		if (combo > 999) combo = 999;

		int digits[3] = { combo / 100,(combo / 10) % 10, combo % 10 };
		float popScale = 1.0f + (m_ComboPopTimer / 0.15f) * 0.4f; // 最大1.4倍まで膨らむ

		float digitW = 40.0f;
		float startX = 200.0f;

		Renderer::SetBlendState(BS_ALPHABLEND);
		for (int i = 0; i < 3; i++)
		{
			m_ComboDigits[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
			m_ComboDigits[i].SetScale(m_NuberSplite_Size * popScale, m_NuberSplite_Size * popScale, 1.0f);
			m_ComboDigits[i].SetPosition(startX + digitW * i, 250.0f, 0.0f);
			m_ComboDigits[i].Draw(cam);
		}
	}

	// ── 判定内訳の表示（PERFECT/GREAT/GOOD/MISS）──
	{
		auto drawCountRow = [&](Texture2D* row, int value, float y)
			{
				if (value > 9999) value = 9999;
				int digits[4] = { value / 1000, value / 100, (value / 10) % 10, value % 10 };
				float digitW = 26.0f;   // コンボより詰めて表示
				float startX = 40.0f;

				for (int i = 0; i < 4; i++)
				{
					row[i].SetUV((float)(digits[i] + 1), 1.0f, 10.0f, 1.0f);
					row[i].SetPosition(startX + digitW * i, y - 100.0f, 0.0f);
					row[i].Draw(cam);
				}
			};
		float topY = 400.0f;	//一番上のY座標
		float rowSpacing = 80.0f;	//行間

		Renderer::SetBlendState(BS_ALPHABLEND);
		drawCountRow(m_PerfectDigits, m_JudgeSystem.GetPerfectCount(), topY);
		drawCountRow(m_GreatDigits, m_JudgeSystem.GetGreatCount(), topY - rowSpacing);
		drawCountRow(m_GoodDigits, m_JudgeSystem.GetGoodCount(), topY - rowSpacing * 2);
		drawCountRow(m_MissDigits, m_JudgeSystem.GetMissCount(), topY - rowSpacing * 3);
		Renderer::SetBlendState(BS_NONE);
	}

	Renderer::SetBlendState(BS_ADDITIVE);
	for (int i = 0; i < 6; i++)
	{
		if (!m_BarrierEffects[i].active) continue;
		//Renderer::SetDepthEnable(false);
		float progress = m_BarrierEffects[i].timer / kBarrierDuration;
		Vector3 pos = m_JudgeCircles[i].GetPosition(m_MusicTime);
		//Vector3 pos = Vector3(0.0f, 0.0f, 1.0f);
		m_BarrierQuad.Draw(pos, 300.0f, progress, GetLaneColor(i), view);
	}
	Renderer::SetBlendState(BS_NONE);
```

追加:
```cpp
	// ── コンボ表示・判定ランク演出・判定内訳・障壁演出は ComboHud にまとめて委譲 ──
	Vector3 laneColors[6];
	for (int i = 0; i < 6; i++) laneColors[i] = GetLaneColor(i);

	m_ComboHud.Draw(cam, view,
		m_JudgeSystem.GetComboCount(),
		m_JudgeSystem.GetPerfectCount(), m_JudgeSystem.GetGreatCount(),
		m_JudgeSystem.GetGoodCount(), m_JudgeSystem.GetMissCount(),
		judgeCircleScreenPos, judgeCirclePos3D, laneColors);
```

以上でChartObject.cppは完了です。

---

## 5. Visual Studioへの新規ファイル追加について

前回同様、`ComboHud.h` / `ComboHud.cpp` をソリューションエクスプローラーで追加すればプロジェクトファイルへの登録は自動で行われます。フィルターは「ソースファイル\Chart」「ヘッダーファイル\Chart」に入れてください。

---

## 6. 期待される削減量

- ChartObject.cpp から、コンボ/判定ランク/判定内訳/障壁まわりのコード（合計150〜200行程度）が無くなります。
- ChartObject.h から、関連するメンバ・構造体・定数（合計15個前後の宣言）が無くなります。
- 新規に ComboHud.h（約55行）・ComboHud.cpp（約150行）ができます。

ビルドしてエラーが出たら、いつも通りスクリーンショットを送ってください。
