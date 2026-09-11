# ClosingRingGuide 追加ガイド

## これは何か

送ってもらった画像のイメージ通り、ノーツごとに「左右から半円ずつ伸びていって、判定時刻ちょうどでぴったり1つの円になる」リングを表示します。

- ノーツが出現した瞬間(spawnTime)から伸び始める
- 右側(3時の方向)と左側(9時の方向)、2箇所から同時に伸び始める
- 判定時刻(またはロングノーツは到達時刻)ちょうどで、上下でつながって完全な円になる
- リングの内側は完全に透明。ノーツ本体やレーンの視界は一切遮らない
- 色はそのノーツのレーン色(`GetLaneColor`)に合わせる
- 複数のリングが画面上で重なっても構わず全部描く。ただし「判定が一番近い(＝一番速く判定が来る)ノーツのリングが必ず一番手前に見える」ように描画順を制御する

判定円まわりの仕組み(判定円の位置・レーンのm_LaneFill等)には一切依存しません。ノーツ本体の現在位置(`NotePath::EvaluateNotePosition`)にそのまま追従する、ノーツ単位の新しい視認性ガイドです。

「判定が速い順に描く」というアイデアについてですが、これは理にかなっています。実装としては、画面奥にある(まだ判定まで時間がある)ノーツのリングを先に描き、判定が近いノーツのリングを最後に描くことで、後から描いた方が上に乗る(ペインターズアルゴリズム)ようにします。ただしこのゲームは3Dの奥行き(デプスバッファ)で前後関係を自動判定しているので、そのままだと「描いた順」ではなく「実際のZ距離」で前後が決まってしまいます。なので、このリングを描いている間だけデプステストを切って(`Enemy::Draw`が常に手前に描くのに使っているのと同じ`Renderer::SetDepthEnable(false)`)、描画順だけで前後関係が決まるようにします。これで「判定が一番速いノーツのリングが、実際の奥行きに関係なく必ず一番手前に見える」という狙い通りの挙動になります。

新規クラス`ClosingRingGuide`と、新規シェーダー`closingRingPS.hlsl`を1つずつ追加します。テクスチャは不要です(色も形も全部シェーダー側で計算する、画像を使わないタイプのリングです)。既存コードの削除はありません。

---

## 1. 新規ファイル：ClosingRingGuide.h

`ChartObject.h`と同じ階層に新規ヘッダーファイルを作成してください。

```cpp
#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include <SimpleMath.h>

// ============================================================
// ClosingRingGuide : ノーツの現在位置に「左右から半円ずつ伸びて、
//   判定時刻ちょうどで1つの円になる」リングを表示するクラス。
//   内側は完全に透明なので、奥から飛んでくるノーツ本体を隠さない。
//   テクスチャ不要（色・形はすべてシェーダーで計算する）。
// ============================================================
class ClosingRingGuide
{
public:
	void Init();

	// center   : リングの中心（ノーツの現在位置をそのまま渡す）
	// scale    : リングの直径
	// progress : 0〜1。1でちょうど円が閉じる（＝判定時刻ちょうど）
	// color    : ノーツ(レーン)の色
	void Draw(const DirectX::SimpleMath::Vector3& center, float scale,
		float progress, const DirectX::SimpleMath::Vector3& color);

private:
	struct ClosingRingParams
	{
		float progress;
		float padding0[3];		//16バイトアライメント
		DirectX::SimpleMath::Vector4 color;
	};	//sizeof == 32

	Shader m_Shader;
	VertexBuffer<VERTEX_3D> m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
};
```

---

## 2. 新規ファイル：ClosingRingGuide.cpp

`ChartObject.cpp`と同じ階層に新規ソースファイルを作成してください。

```cpp
#include "ClosingRingGuide.h"

using namespace DirectX::SimpleMath;

void ClosingRingGuide::Init()
{
	std::vector<VERTEX_3D> verts(4);
	verts[0].position = Vector3(-0.5f, 0.5f, 0.0f);
	verts[1].position = Vector3(0.5f, 0.5f, 0.0f);
	verts[2].position = Vector3(-0.5f, -0.5f, 0.0f);
	verts[3].position = Vector3(0.5f, -0.5f, 0.0f);
	for (int i = 0; i < 4; i++)
	{
		verts[i].normal = Vector3(0.0f, 0.0f, -1.0f);
		verts[i].color = Color(1.0f, 1.0f, 1.0f, 1.0f);
	}
	verts[0].uv = Vector2(0.0f, 0.0f);
	verts[1].uv = Vector2(1.0f, 0.0f);
	verts[2].uv = Vector2(0.0f, 1.0f);
	verts[3].uv = Vector2(1.0f, 1.0f);
	m_VertexBuffer.Create(verts);

	std::vector<unsigned int> idx = { 0, 1, 2, 3 };
	m_IndexBuffer.Create(idx);

	// unlitTextureVS.hlslはテクスチャ座標を渡すだけの汎用VSなので流用
	// （PS側はテクスチャをサンプルせず、UVから直接リングを計算する）
	m_Shader.Create("shader/unlitTextureVS.hlsl", "shader/closingRingPS.hlsl");
	m_Shader.CreatePSConstantBuffer(sizeof(ClosingRingParams));
}

void ClosingRingGuide::Draw(const Vector3& center, float scale, float progress, const Vector3& color)
{
	if (progress < 0.0f) progress = 0.0f;
	if (progress > 1.0f) progress = 1.0f;

	Matrix world = Matrix::CreateScale(scale) * Matrix::CreateTranslation(center);
	Renderer::SetWorldMatrix(&world);

	m_Shader.SetGPU();

	ClosingRingParams parms = {};
	parms.progress = progress;
	parms.color = Vector4(color.x, color.y, color.z, 1.0f);
	m_Shader.UpdatePSConstantBuffer(&parms, sizeof(parms));

	m_VertexBuffer.SetGPU();
	m_IndexBuffer.SetGPU();
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->DrawIndexed(4, 0, 0);
}
```

---

## 3. 新規シェーダーファイル：shader/closingRingPS.hlsl

`shader`フォルダの中に、既存の`magicCireclePS.hlsl`などと同じ階層で新規ファイルを作成してください。

```hlsl
#include "common.hlsl"

cbuffer ClosingRingParams : register(b0) // PS用スロットはVSのb0-b5と別枠なので0でOK（BarrierBufferと同じ考え方）
{
    float progress;   // 0〜1。1でちょうど円が閉じる
    float3 padding0;
    float4 ringColor;
}

static const float PI = 3.14159265f;
static const float HALF_PI = 1.57079632f;

float4 main(in PS_IN input) : SV_TARGET
{
    float2 centered = (input.tex - 0.5) * 2.0; // -1〜1
    float dist = length(centered);

    // ── リング状にクリップ（内側は完全に透明＝ノーツの邪魔をしない）──
    const float outerR = 1.0;
    const float innerR = 0.72; // リングの太さ。細くしたければ1.0に近づける、太くしたければ0に近づける
    if (dist > outerR || dist < innerR)
        discard;

    // 右(3時)を角度0、左(9時)を角度πとして、そこからの角度距離を求める
    float angle = atan2(centered.y, centered.x); // -π〜π
    float distFromRight = abs(angle);
    float distFromLeft = PI - distFromRight;

    // progressに応じて、右と左それぞれの起点から上下に伸びていく角度範囲
    float grow = progress * HALF_PI;

    bool visibleRight = distFromRight <= grow;
    bool visibleLeft = distFromLeft <= grow;
    if (!visibleRight && !visibleLeft)
        discard;

    // 伸びている先端をうっすら光らせる（無くても良ければこの3行とその下のcolor計算を単純化してOK）
    float edge = min(abs(distFromRight - grow), abs(distFromLeft - grow));
    float glow = smoothstep(0.10, 0.0, edge);

    float3 color = ringColor.rgb + glow * 0.6;
    return float4(color, 1.0);
}
```

ソリューションエクスプローラーで`.hlsl`ファイルを追加する際は、既存の`magicCireclePS.hlsl`などと同じフィルター(shaderフォルダ)に入れておいてください。`.cpp`/`.h`と違ってプロジェクトのビルド対象への自動追加が効かない場合があるので、追加後に一度ビルドしてファイルが認識されているか確認してください（もし認識されない場合は教えてください、プロパティの設定方法を案内します）。

---

## 4. ChartObject.h の変更

### include追加

以下の行を探してください。

```cpp
#include "TimingMatchGuide.h"
```

その直後に追加してください。

```cpp
#include "TimingMatchGuide.h"
#include "ClosingRingGuide.h"
```

### メンバ変数追加

以下の行を探してください（前回追加した`m_TimingGuide`の直後）。

```cpp
	TimingMatchGuide m_TimingGuide;	// 育つ四角×縮む四角が一致した瞬間＝ジャストタイミングを伝えるガイド
```

その直後に追加してください。

```cpp
	TimingMatchGuide m_TimingGuide;	// 育つ四角×縮む四角が一致した瞬間＝ジャストタイミングを伝えるガイド
	ClosingRingGuide m_ClosingRingGuide;	// ノーツごとに左右から伸びて判定時刻ちょうどで円になるリング
```

---

## 5. ChartObject.cpp の変更

### 5-1. Init()：シェーダーの初期化追加

以下の行を探してください（前回追加した`m_TimingGuide.Init(...)`の行）。

```cpp
	m_TimingGuide.Init("assets/texture/timing_grow.png", "assets/texture/timing_shrink.png");
	m_ComboHud.Init();
```

これを、次のように変更してください。

```cpp
	m_TimingGuide.Init("assets/texture/timing_grow.png", "assets/texture/timing_shrink.png");
	m_ClosingRingGuide.Init();
	m_ComboHud.Init();
```

### 5-2. Draw()：新しいリングの描画ループを追加

削除：なし

`Draw()`内の「接近リング」ブロックの直後、次の行を探してください（「レーンごとに一番近い未判定ノーツの接近度」のコメント行の直前です）。

```cpp
	Renderer::SetBlendState(BS_NONE);

	// ── レーンごとに一番近い未判定ノーツの接近度(0〜1)を計算 ──
```

これを、次のように変更してください（新しいブロックをまるごと挿入するだけです）。

```cpp
	Renderer::SetBlendState(BS_NONE);

	// ── ノーツ到達ガイド：左右から伸びて判定時刻ちょうどで円になるリング ──
	// フリックは対象外（接近リングと同じ扱い）。
	// 判定が一番近いノーツのリングが必ず一番手前に見えるよう、
	// 判定が遅い順に描いてから、判定が速いものを最後に描く。
	{
		std::vector<const ChartNote*> ringNotes;
		for (auto& note : m_Notes)
		{
			if (note.judged) continue;
			if (note.IsFlick()) continue;

			float st = note.GetSpawnTime();
			float et = note.IsLong() ? note.arrivalTime : note.time;
			if (m_MusicTime < st || m_MusicTime > et) continue;

			ringNotes.push_back(&note);
		}

		std::sort(ringNotes.begin(), ringNotes.end(),
			[](const ChartNote* a, const ChartNote* b)
			{
				float ta = a->IsLong() ? a->arrivalTime : a->time;
				float tb = b->IsLong() ? b->arrivalTime : b->time;
				return ta > tb; // 判定が遅い(時間が大きい)方を先に描く
			});

		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::SetDepthEnable(false); // 実際の奥行きではなく、描画順だけで前後関係を決める
		for (const ChartNote* notePtr : ringNotes)
		{
			const ChartNote& note = *notePtr;
			float st = note.GetSpawnTime();
			float et = note.IsLong() ? note.arrivalTime : note.time;
			float duration = et - st;
			float t = (duration > 0.0001f) ? (m_MusicTime - st) / duration : 1.0f;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;

			Vector3 pos = note.IsLong()
				? NotePath::EvaluateLongNotePosition(note, m_MusicTime)
				: NotePath::EvaluateNotePosition(note, m_MusicTime);

			Vector3 laneColor = GetLaneColor(note.lane);
			m_ClosingRingGuide.Draw(pos, 120.0f, t, laneColor);
		}
		Renderer::SetDepthEnable(true);
		Renderer::SetBlendState(BS_NONE);
	}

	// ── レーンごとに一番近い未判定ノーツの接近度(0〜1)を計算 ──
```

---

## 6. 動作確認・調整のポイント

- ノーツが出現した瞬間から、右と左それぞれから半円が伸び始め、判定時刻ちょうどで上下がつながって完全な円になれば成功です。
- リングの太さは`closingRingPS.hlsl`の`innerR`(今0.72)を変えるだけで調整できます。C++側は触らなくてOKです。
- リングのサイズは`ChartObject.cpp`側の`m_ClosingRingGuide.Draw(pos, 120.0f, t, laneColor);`の`120.0f`を変えれば調整できます。
- 複数のノーツが画面上で重なったとき、判定が一番近いものが一番手前に見えているか確認してください。もし前後関係が期待と違う場合は、ソートの向き(`ta > tb`)を逆にすれば直ります。

## 7. 今までの判定ガイドとの整理について

「ガラッと変える」とのことだったので、既存のもの(接近リング`m_ApproachRing`、育つ四角×縮む四角`m_TimingGuide`)は今回消さずに残してあります。このリングと同時に表示すると逆にごちゃつく可能性があるので、まずは比較のために以下の行を一時的にコメントアウトして、このリング単体でどう見えるか確認することをおすすめします。

```cpp
// m_ApproachRing.Draw(pos, ringScale, ringAlpha);
```
```cpp
// m_TimingGuide.Draw(cam, i, screenPos, laneT[i]);
```

見え方を確認したうえで、「このリングだけで十分」「両方あった方がいい」など判断してもらえれば、要らない方を正式に削除するガイドを出します。

以上です。ビルド通ったら見た目を教えてください。
