# Effekseer（エフェクシア）導入手順

時間がある時に、上から順に進めるためのメモ。**event_bg ブランチ（なければ新しいブランチ）でやる**こと。

「要確認」と書いた所は、Effekseer のバージョンで名前が変わっていることがあるので、実物を見て合わせる。

---

## 0. 始める前に

- **先生に確認する。** 提出の条件は「C++ / DirectX」。外部のエフェクトの仕組み（ミドルウェア）を使っていいか、先に聞いておく。
- **ライセンス：** MIT。使ってよいが、クレジット（タイトル画面の下やリードミー）に「Effekseer」とライセンスの表記を入れる。
- **時間の目安：** ランタイムのビルドとプロジェクトへの組み込みで、半日くらい。**半日で動かなかったら、いったんやめて自前の演出に戻る。**
- **すみ分け：** ロングの盾のヒビ、攻撃の光、必殺技のビームなど、ゲームの芯の演出は自前（LongBeamRibbon）のままにする。Effekseer は、細かい粒・斬撃の飾り・背景の飾りなど、作るのに時間がかかる所だけに使う。自前で作った部分は、アピールになる。

---

## 1. 今のプロジェクトの設定（パソコンで確認済み）

ランタイムは、この設定とそろえてビルドしないとリンクでエラーになる。

| 項目 | 今の設定 |
|---|---|
| Visual Studio | 2022（プラットフォームツールセット v143） |
| プラットフォーム | x64 |
| ランタイムライブラリ | Debug：`/MTd`（マルチスレッド デバッグ）、Release：`/MT`（マルチスレッド）。**DLL 版（/MD）ではない** |
| C++ の版 | C++20 |
| 文字セット | マルチバイト（x64） |

---

## 2. エフェクトを作るツール

1. 公式サイト（https://effekseer.github.io/ ）から、Windows 用のツールをダウンロードして解凍する。
2. ツールでエフェクトを作り、`.efkefc` で保存する。
3. 作る時のコツ：
   - ツールの 1 は、このゲームではとても小さい。ゲーム側で 50〜100 倍にする前提で作る（後で `SetScale` で合わせる）。
   - 色を白っぽく作っておくと、ゲーム側でレーンの色に染められる（`SetAllColor` は掛け算）。
   - 短く終わるもの（0.3〜0.5 秒）にする。叩くたびに出るので、長いと画面に溜まる。

---

## 3. ランタイム（ゲームで再生する部分）をビルドする

### 3-1. ソースを取ってくる

GitHub の Effekseer のリポジトリ（https://github.com/effekseer/Effekseer ）から取る。サブモジュールがあるので、`--recursive` を付ける。

```
git clone --recursive https://github.com/effekseer/Effekseer.git
```

Releases のページに、ランタイムだけの zip があれば、そちらでもよい（要確認）。**ツールと同じバージョンのランタイムを使う。** バージョンが違うと、エフェクトが読めないことがある。

### 3-2. CMake でプロジェクトを作る

「Developer PowerShell for VS 2022」を開いて、取ってきたフォルダで実行する。

```
cmake -S . -B build_dx11 -G "Visual Studio 17 2022" -A x64 ^
  -DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF ^
  -DBUILD_DX12=OFF -DBUILD_VULKAN=OFF ^
  -DBUILD_EXAMPLES=OFF -DBUILD_VIEWER=OFF -DBUILD_EDITOR=OFF
```

（PowerShell では、行の最後の `^` を `` ` ``（バッククォート）に変えるか、1 行にまとめる。）

**要確認：オプションの名前。** 一番上の `CMakeLists.txt` を開いて、`option(` で検索すると、使えるオプションと最初の値が全部並んでいる。名前が違ったら、そちらに合わせる。特に大事なのは、次の 2 つ。

- ランタイムを DLL 版にしない（/MT にする）オプション。`OFF` にする。
- DirectX11 の描画（EffekseerRendererDX11）を作るオプション。これは `ON` のままにする。

**オプションで /MT にできなかった時：** できた `build_dx11` の中の `.sln` を Visual Studio で開く。`Effekseer` と `EffekseerRendererDX11`（ほかに `EffekseerRendererCommon` などがあればそれも）のプロジェクトのプロパティで、ランタイムライブラリを、Debug は `/MTd`、Release は `/MT` に手で変える。

### 3-3. ビルドする

```
cmake --build build_dx11 --config Debug
cmake --build build_dx11 --config Release
```

`build_dx11` の中に、Debug 用と Release 用の `.lib` ができる。`Effekseer.lib` と `EffekseerRendererDX11.lib` が中心（要確認：ほかにもできた `.lib` があれば控えておく。Debug 版は名前の最後に `d` が付くことがある）。

---

## 4. プロジェクトに入れる

### 4-1. ファイルを置く

ゲームのフォルダに、こう置く（例）。

```
3Dゲーム作成1/
  external/Effekseer/
    include/          ← ヘッダー
    lib/Debug/        ← Debug で作った .lib
    lib/Release/      ← Release で作った .lib
  assets/effekseer/   ← .efkefc と、エフェクトが使う画像
```

**ヘッダー：** ソースの `Dev/Cpp/Effekseer/` と `Dev/Cpp/EffekseerRendererDX11/` の中身（`.h` とフォルダ）を `include/` にコピーする（要確認：場所はバージョンで少し違うことがある。`Effekseer.h` と `EffekseerRendererDX11.h` があるフォルダを探す）。`EffekseerRendererCommon` などのフォルダを読みに行くエラーが出たら、それも同じようにコピーする。

### 4-2. Visual Studio の設定（x64 の Debug と Release の両方）

プロジェクトのプロパティで、次の 3 つを足す。

| 場所 | 足す値 |
|---|---|
| C/C++ → 全般 → 追加のインクルードディレクトリ | `$(ProjectDir)external\Effekseer\include` |
| リンカー → 全般 → 追加のライブラリディレクトリ | Debug：`$(ProjectDir)external\Effekseer\lib\Debug`、Release：`$(ProjectDir)external\Effekseer\lib\Release` |
| リンカー → 入力 → 追加の依存ファイル | `Effekseer.lib;EffekseerRendererDX11.lib;`（3-3 で控えた名前に合わせる） |

### 4-3. エフェクトを置く

`.efkefc` と、そのエフェクトが使っている画像を、`assets/effekseer/` に置く。**画像は `.efkefc` から見た場所で探される**ので、ツールで作った時のフォルダの形のままコピーする。

---

## 5. 再生するクラス（EffekseerFx）

GameEvents の「叩いた」を聞いて、判定円の位置で斬撃を出す。**ChartObject は触らない。**

### EffekseerFx.h

```cpp
#pragma once
#include "Object.h"
#include <chrono>
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>

//Effekseerで作ったエフェクトを再生する。GameEventsを聞いて出す
class EffekseerFx : public Object
{
public:
	void Init() override;
	void Update() override;
	void Draw(Camera* cam) override;
	void Uninit() override;
	Camera::Layer GetLayer() const override { return Camera::Layer::Playfield; }	//判定円と同じカメラで描く

private:
	Effekseer::ManagerRef m_Manager;
	EffekseerRenderer::RendererRef m_Renderer;	//要確認：型の名前（EffekseerRendererDX11::RendererRef の版もある）
	Effekseer::EffectRef m_Slash;		//普通の斬撃
	Effekseer::EffectRef m_SlashWeak;	//WEAKの十字の斬撃

	int m_JudgeSub = 0;	//GameEventsの登録番号

	std::chrono::steady_clock::time_point m_LastClock;
	bool m_ClockStarted = false;

	static constexpr float kScale = 60.0f;	//ツールの1をゲームの何倍にするか
};
```

### EffekseerFx.cpp

```cpp
#include "EffekseerFx.h"
#include "Renderer.h"
#include "Camera.h"
#include "GameEvents.h"
#include <cstring>

void EffekseerFx::Init()
{
	//描画する人と、エフェクトを管理する人を作る（数字は同時に出せる量の上限）
	m_Renderer = EffekseerRendererDX11::Renderer::Create(Renderer::GetDevice(), Renderer::GetDeviceContext(), 8000);
	m_Manager = Effekseer::Manager::Create(8000);
	m_Manager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);	//このゲームは左手系

	//描き方と読み込み方を、描画する人から借りる（公式のサンプルと同じ）
	m_Manager->SetSpriteRenderer(m_Renderer->CreateSpriteRenderer());
	m_Manager->SetRibbonRenderer(m_Renderer->CreateRibbonRenderer());
	m_Manager->SetRingRenderer(m_Renderer->CreateRingRenderer());
	m_Manager->SetTrackRenderer(m_Renderer->CreateTrackRenderer());
	m_Manager->SetModelRenderer(m_Renderer->CreateModelRenderer());
	m_Manager->SetTextureLoader(m_Renderer->CreateTextureLoader());
	m_Manager->SetModelLoader(m_Renderer->CreateModelLoader());
	m_Manager->SetMaterialLoader(m_Renderer->CreateMaterialLoader());
	m_Manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

	//エフェクトを読む（パスは u"..."：char16_tの文字列）
	m_Slash = Effekseer::Effect::Create(m_Manager, u"assets/effekseer/slash.efkefc");
	m_SlashWeak = Effekseer::Effect::Create(m_Manager, u"assets/effekseer/slash_weak.efkefc");
	if (m_Slash == nullptr) OutputDebugStringA("[EffekseerFx] slash.efkefc not found\n");

	//「叩いた」を聞いて、判定円の位置で斬撃を出す
	m_JudgeSub = GameEvents::OnJudge().Subscribe([this](const GameEvents::Judge& e)
		{
			auto& eff = (e.rank == 0 && m_SlashWeak != nullptr) ? m_SlashWeak : m_Slash;
			if (eff == nullptr) return;
			Effekseer::Handle h = m_Manager->Play(eff, e.pos.x, e.pos.y, e.pos.z);
			m_Manager->SetScale(h, kScale, kScale, kScale);
			m_Manager->SetAllColor(h, Effekseer::Color(
				(uint8_t)(e.color.x * 255.0f), (uint8_t)(e.color.y * 255.0f), (uint8_t)(e.color.z * 255.0f), 255));
		});
}

void EffekseerFx::Update()
{
	auto now = std::chrono::steady_clock::now();
	if (!m_ClockStarted) { m_LastClock = now; m_ClockStarted = true; }
	float dt = std::chrono::duration<float>(now - m_LastClock).count();
	m_LastClock = now;

	m_Manager->Update(dt * 60.0f);	//Effekseerは「1 = 1/60秒」で数える
}

void EffekseerFx::Draw(Camera* cam)
{
	cam->SetCamera(0);	//3Dのカメラにする（ビュー行列と投影行列が作られる）

	//SimpleMathの行列を、そのままEffekseerの行列に写す（どちらも4x4のfloatで並びが同じ）
	Effekseer::Matrix44 view, proj;
	std::memcpy(view.Values, &cam->GetViewMatrix(), sizeof(float) * 16);
	std::memcpy(proj.Values, &cam->GetProjectionMatrix(), sizeof(float) * 16);
	m_Renderer->SetCameraMatrix(view);
	m_Renderer->SetProjectionMatrix(proj);

	m_Renderer->BeginRendering();
	m_Manager->Draw();
	m_Renderer->EndRendering();
}

void EffekseerFx::Uninit()
{
	GameEvents::OnJudge().Unsubscribe(m_JudgeSub);
	m_Manager->StopAllEffects();
	m_Slash.Reset();
	m_SlashWeak.Reset();
	m_Manager.Reset();
	m_Renderer.Reset();
}
```

### Stage1Scene に足す

`#include "EffekseerFx.h"` を足し、Init の ChartObject を足した後（ノーツより前に描かれないように）に追加する。

```cpp
	auto* efk = Game::GetInstance()->AddObject<EffekseerFx>();
	m_MySceneObjects.emplace_back(efk);
```

---

## 6. つまずきやすい所

| 症状 | 原因と直し方 |
|---|---|
| リンクで `LNK2038: RuntimeLibrary の不一致` | ランタイムが DLL 版（/MD）でビルドされている。3-2 に戻って /MT・/MTd でビルドし直す |
| リンクで `_ITERATOR_DEBUG_LEVEL の不一致` | Debug のゲームに Release の lib（またはその逆）をつないでいる。ライブラリディレクトリの Debug / Release を確認 |
| `Effekseer.h が見つからない` | インクルードディレクトリの設定が、x64 の Debug と Release の両方に入っているか確認 |
| エフェクトが出ない | `.efkefc` のパス、`kScale`（小さすぎて見えない）、判定円の位置がカメラに入っているか |
| エフェクトは出るが、画像が真っ白・真っ黒 | 画像が `.efkefc` から見た場所にない。ツールで作った時のフォルダの形のままコピーする |
| エフェクトを出した後、ほかの物の見た目がおかしくなる | Effekseer が描く時にブレンドなどの設定を変えている。ふつうは終わったら元に戻すが、直らなければ Draw の後でゲーム側の設定（`Renderer::SetBlendState` など）を戻す |
| 動きが速すぎる・遅すぎる | `Update` に渡す値。秒ではなく「1/60 秒が何回分か」を渡す |
| 左右・前後が逆 | `SetCoordinateSystem(LH)` を忘れていないか |

---

## 7. 次に足せるもの（GameEvents を聞くだけ）

- `OnShieldBreak`：盾が割れた場所で、大きな破片のエフェクト。
- `OnBeat`：小節の頭（`isBar`）で、背景に光の輪。
- `OnLongEnd`：ロングを押し切った時に、判定円から光の柱。

どれも EffekseerFx の Init に `Subscribe` を 1 つ足し、Uninit に `Unsubscribe` を 1 つ足すだけで、ChartObject は触らない。
