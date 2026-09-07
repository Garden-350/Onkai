# TimingMatchGuide 追加ガイド

## これは何か

判定円の「今どこにあるか」に頼らず、「今このレーンをいつ叩けばジャストか」を伝えるための新しい表示です。

判定円の画面座標(screenPos)を中心に、四角のスプライトを2枚重ねて表示します。

- **育つ四角**：小さい状態から始まり、判定時刻に向けてだんだん大きくなる
- **縮む四角**：大きい状態から始まり、判定時刻に向けてだんだん小さくなる

この2枚がぴったり同じ大きさになった瞬間＝ジャストタイミング、という見た目のガイドです。判定円自体の位置(判定円がどこにアニメーションしていても)にそのまま追従するので、既存のキーフレームアニメーションには一切手を加えません。

今回は「判定円が重なったときにどうするか」は含めていません（ご本人の希望通り、そこは追々）。まずは1レーン単体で見やすくなるかを確認する版です。

新規クラス `TimingMatchGuide` を1つ追加し、`ChartObject` には「初期化の1行」「描画ループへの1行」を足すだけです。削除する既存コードはありません。

---

## 1. 新規ファイル：TimingMatchGuide.h

ソリューションエクスプローラーで `ChartObject.h` と同じ階層に新規ヘッダーファイル `TimingMatchGuide.h` を作成し、以下の内容をそのまま貼り付けてください。

```cpp
#pragma once
#include "Texture2D.h"
#include <SimpleMath.h>

class Camera;

// ============================================================
// TimingMatchGuide : 判定円ごとに「だんだん小さくなる四角」と
//   「だんだん大きくなる四角」を表示し、判定時刻ちょうどで
//   両者のサイズがぴったり重なるようにすることで、
//   位置のアニメーションに頼らずジャストタイミングを伝えるガイド。
//   判定円の画面座標(screenPos)にそのまま追従して表示される。
// ============================================================
class TimingMatchGuide
{
public:
	// growTexturePath   : 小さい状態から育っていく四角のテクスチャ
	// shrinkTexturePath : 大きい状態から縮んでいく四角のテクスチャ
	void Init(const char* growTexturePath, const char* shrinkTexturePath);

	// laneIndex : 0〜5
	// screenPos : 判定円の画面座標（ChartObject::Draw内で計算済みのjudgeCircleScreenPos[i]、
	//             もしくは同じループ内のscreenPosをそのまま渡す）
	// t         : そのレーンで一番近い未判定ノーツの接近度(0〜1)。1で判定時刻ちょうど
	void Draw(Camera* cam, int laneIndex, const DirectX::SimpleMath::Vector3& screenPos, float t);

private:
	Texture2D m_GrowSquare[6];
	Texture2D m_ShrinkSquare[6];

	// ── サイズ調整はここの3つの定数だけで完結します ──
	static constexpr float kMatchSize = 100.0f;   // t=1でぴったり一致するサイズ（＝テクスチャの想定サイズ）
	static constexpr float kGrowStart = 20.0f;    // 育つ四角：開始サイズ
	static constexpr float kShrinkStart = 260.0f; // 縮む四角：開始サイズ（レーン間隔より大きいと、出始めに隣のレーンとかぶるので注意）
};
```

---

## 2. 新規ファイル：TimingMatchGuide.cpp

同じく `ChartObject.cpp` と同じ階層に新規ソースファイル `TimingMatchGuide.cpp` を作成し、以下の内容をそのまま貼り付けてください。

```cpp
#include "TimingMatchGuide.h"

using namespace DirectX::SimpleMath;

void TimingMatchGuide::Init(const char* growTexturePath, const char* shrinkTexturePath)
{
	for (int i = 0; i < 6; i++)
	{
		m_GrowSquare[i].Init();
		m_GrowSquare[i].SetTexture(growTexturePath);
		m_GrowSquare[i].SetScale(kGrowStart, kGrowStart, 1.0f);

		m_ShrinkSquare[i].Init();
		m_ShrinkSquare[i].SetTexture(shrinkTexturePath);
		m_ShrinkSquare[i].SetScale(kShrinkStart, kShrinkStart, 1.0f);
	}
}

void TimingMatchGuide::Draw(Camera* cam, int laneIndex, const Vector3& screenPos, float t)
{
	if (laneIndex < 0 || laneIndex >= 6) return;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// 育つ四角：kGrowStart → kMatchSize
	float growSize = kGrowStart + t * (kMatchSize - kGrowStart);
	// 縮む四角：kShrinkStart → kMatchSize
	float shrinkSize = kShrinkStart - t * (kShrinkStart - kMatchSize);

	m_GrowSquare[laneIndex].SetScale(growSize, growSize, 1.0f);
	m_GrowSquare[laneIndex].SetPosition(screenPos);
	m_GrowSquare[laneIndex].Draw(cam);

	m_ShrinkSquare[laneIndex].SetScale(shrinkSize, shrinkSize, 1.0f);
	m_ShrinkSquare[laneIndex].SetPosition(screenPos);
	m_ShrinkSquare[laneIndex].Draw(cam);
}
```

ソリューションエクスプローラー上で追加すれば `golf.vcxproj` / `golf.vcxproj.filters` への登録はVisual Studioが自動でやってくれるので、手動編集は不要です（これまでの切り出しと同じです）。

---

## 3. ChartObject.h の変更

### 3-1. include追加

削除：なし

以下の行を探してください（インクルード列の最後の方、13行目あたり）。

```cpp
#include "ComboHud.h"
```

その直後に、次の1行を追加してください。

```cpp
#include "ComboHud.h"
#include "TimingMatchGuide.h"
```

### 3-2. メンバ変数追加

削除：なし

以下の行を探してください（`m_LaneFill`の宣言、178行目あたり）。

```cpp
	Texture2D m_LaneFill[6];		// 判定円の内側（接近度に応じて育つ・フェードイン）
```

その直後に、次の1行を追加してください。

```cpp
	Texture2D m_LaneFill[6];		// 判定円の内側（接近度に応じて育つ・フェードイン）
	TimingMatchGuide m_TimingGuide;	// 育つ四角×縮む四角が一致した瞬間＝ジャストタイミングを伝えるガイド
```

---

## 4. ChartObject.cpp の変更

### 4-1. Init()：テクスチャの読み込み追加

削除：なし

`Init()`内で以下のブロックを探してください（「タイミングガイド：外枠と育つ四角のセットアップ」の直後）。

```cpp
		m_LaneFill[i].Init();
		m_LaneFill[i].SetTexture(guideTexturePath_in);
		m_LaneFill[i].SetScale(m_FrameSize / 10, m_FrameSize / 10, 1.0f);
	}
	m_ComboHud.Init();
}
```

これを、次のように変更してください（`m_TimingGuide.Init(...)`の1行を`m_ComboHud.Init();`の前に追加するだけです）。

```cpp
		m_LaneFill[i].Init();
		m_LaneFill[i].SetTexture(guideTexturePath_in);
		m_LaneFill[i].SetScale(m_FrameSize / 10, m_FrameSize / 10, 1.0f);
	}
	// ── ジャストタイミングガイド（育つ四角×縮む四角）のテクスチャ読み込み ──
	// ※ パスは仮です。用意したテクスチャの実際のファイル名に置き換えてください
	m_TimingGuide.Init("assets/texture/timing_grow.png", "assets/texture/timing_shrink.png");
	m_ComboHud.Init();
}
```

### 4-2. Draw()：レーンごとの描画ループに追加

削除：なし

`Draw()`内、判定円のループの中、`if (laneActive[i])`ブロックの中で以下の行を探してください（`m_LaneFill[i].Draw(cam);`の行、938行目あたり）。

```cpp
			m_LaneFill[i].Draw(cam);
		}
```

これを、次のように変更してください（`m_TimingGuide.Draw(...)`の1行を追加するだけです。この`{ }`の中は`if (laneActive[i])`のブロックなので、インデントの位置に注意してください）。

```cpp
			m_LaneFill[i].Draw(cam);

			// ── 一致でジャストタイミングを伝えるガイド（育つ四角×縮む四角）──
			m_TimingGuide.Draw(cam, i, screenPos, laneT[i]);
		}
```

---

## 5. 動作確認のポイント

- ノーツが出現してから判定時刻に近づくにつれて、大きい四角が縮み、小さい四角が育っていき、判定時刻ちょうどで同じ大きさになるはずです。
- `laneActive[i]`が真の間だけ描画されるので、そのレーンに未判定ノーツが無い間は何も表示されません（＝今回はまだ「常時表示」ではなく、ノーツが飛んでくる間だけ表示、という形になっています。常時表示にしたくなったら`laneActive[i]`の条件を外す方向で調整できます）。
- `kShrinkStart = 260.0f`は初期値なので、実際に見てみて大きすぎる／小さすぎる場合は`TimingMatchGuide.h`の`kGrowStart` / `kShrinkStart` / `kMatchSize`の数値だけ調整すれば挙動が変わります（cpp側は触らなくてOKです）。
- ビルドが通ったら、`assets/texture/timing_grow.png` / `timing_shrink.png`を実際に用意したテクスチャのパスに差し替えてください（ファイル名は仮なので何でも構いません、`Init()`の呼び出し側の文字列だけ合わせればOKです）。

以上です。ビルドが通ったら教えてください。重なったときの見え方は、次のステップとしていつでもどうぞ。
