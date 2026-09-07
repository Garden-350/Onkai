# JudgeSystem 切り出しガイド

ChartObject.cpp/.h から「判定ロジックと判定カウント」を JudgeSystem という新しいクラスに切り出す手順です。実際の編集はそちらで行う前提で、削除箇所・追加箇所を正確に書いています。

## 設計方針（なぜこの形にしたか）

- JudgeLane / JudgeFlickNotes / JudgeLongNoteHold / UpdateAutoJudgge と、判定カウント（Perfect/Great/Good/Miss/Combo/MaxCombo）を JudgeSystem に移す。
- パーティクル・ライトエフェクト・判定ランクのポップ演出・障壁演出（コンボ加算後の「見た目」の部分）は ChartObject に残す。JudgeSystem は「ヒットした」「空振りした」という事実をコールバック経由で ChartObject に伝えるだけにする。
- こうすることで JudgeSystem は m_Particle や m_LightEffects や m_JudgeRankEffects などを一切知らなくてよくなり、依存が少なく安全に切り出せる（HomingBeamSystem を切り出したときと同じ考え方）。
- Player の参照はJudgeSystemには持たせず、ChartObject側で `GetSide()` を呼んだ結果（int playerSide）だけを渡す。

### 地味だが重要な仕様の確認（動作を変えないために）

- `LoadChart()` は現状 `m_Perfect/m_Great/m_Good/m_Miss` だけを0にリセットしていて、`m_Combo` と `m_MaxCombo` はリセットしていません（バグかもしれませんが今回は現状維持します）。そのため JudgeSystem 側のリセット関数も Perfect/Great/Good/Miss だけをリセットします。
- `Update()` の中に「TAPノーツの自動ミス判定」の**まったく同じループが2回**あります（1回目は無条件、2回目はelse節=手動モード時のみ）。2回目は1回目で既に judged=true になっているので実質空振りですが、これも現状維持のため両方とも移植します（`MarkTapAutoMiss` という関数にして2箇所から呼びます）。

---

## 1. 新規ファイル: JudgeSystem.h

新規作成して、以下の内容をそのまま保存してください（文字コード: UTF-8 with BOM、改行: CRLF ※他のChart系ファイルと合わせてください）。

```cpp
#pragma once
#include <vector>
#include <functional>

struct ChartNote;
class Sound;

// ============================================================
// JudgeSystem : TAP/FLICK/LONGNOTEの判定ロジックと、判定カウント
//   (Perfect/Great/Good/Miss/Combo/MaxCombo)を担当するクラス。
//   パーティクル・ライトエフェクト・判定ランク演出などの「見た目」は
//   ここでは一切扱わず、コールバック経由でChartObject側に委譲する。
// ============================================================
class JudgeSystem
{
public:
	// ヒット成立時に呼ばれる (laneIndex, rank: 0=PERFECT,1=GREAT,2=GOOD)
	using OnJudgeFn = std::function<void(int laneIndex, int rank)>;
	// TAPノーツが無いレーンを空押ししたときに呼ばれる
	using OnEmptyPressFn = std::function<void(int laneIndex)>;

	void SetOnJudge(OnJudgeFn fn) { m_OnJudge = fn; }
	void SetOnEmptyPress(OnEmptyPressFn fn) { m_OnEmptyPress = fn; }

	// TAPノーツ：キーを押した瞬間の判定
	void JudgeLane(int key, int laneIndex, std::vector<ChartNote>& notes, float musicTime, Sound& sound);
	// FLICKノーツ：時間ウィンドウ内でサイド一致を判定
	void JudgeFlickNotes(std::vector<ChartNote>& notes, float musicTime, int playerSide);
	// LONGNOTE：押し始め〜押しっぱなし〜離すを判定
	void JudgeLongNoteHold(std::vector<ChartNote>& notes, float musicTime);
	// オートモード用：到達時刻を過ぎたらパーフェクトに確定させる
	void UpdateAutoJudgge(std::vector<ChartNote>& notes, float musicTime, Sound& sound);
	// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	void MarkTapAutoMiss(std::vector<ChartNote>& notes, float musicTime);

	// LoadChart時に呼ぶ：Perfect/Great/Good/Missのみリセット（Combo/MaxComboは元の仕様通りリセットしない）
	void ResetJudgeCounts();

	int GetPerfectCount() const { return m_Perfect; }
	int GetGreatCount()   const { return m_Great; }
	int GetGoodCount()    const { return m_Good; }
	int GetMissCount()    const { return m_Miss; }
	int GetMaxComboCount() const { return m_MaxCombo; }
	int GetComboCount()   const { return m_Combo; } // Draw()でのコンボ数字表示用

private:
	void RegisterHit(int laneIndex, int rank); // m_Combo++ / m_MaxCombo更新 + m_OnJudgeコールバック呼び出し

	int m_Perfect = 0;
	int m_Great = 0;
	int m_Good = 0;
	int m_Miss = 0;
	int m_Combo = 0;
	int m_MaxCombo = 0;

	OnJudgeFn m_OnJudge;
	OnEmptyPressFn m_OnEmptyPress;

	// ── 判定タイミングの許容誤差（秒）。値が小さいほどシビア ──
	static constexpr float kPerfect = 0.2f;
	static constexpr float kGreat = 0.4f;
	static constexpr float kGud = 0.6f;			// この範囲を過ぎたら自動ミス
	static constexpr float kLongTickInterval = 0.05f;	// ロングノーツの継続判定(tick)間隔
};
```

---

## 2. 新規ファイル: JudgeSystem.cpp

```cpp
#include "JudgeSystem.h"
#include "ChartObject.h"	// ChartNote の完全な定義を得るため
#include "Input.h"
#include "sound.h"
#include <algorithm>
#include <cmath>

namespace
{
	// レーン(0〜5) <-> S/D/F/J/K/Lキーの対応表（ChartObject.cppの同名定数と同じ値）
	constexpr int kLaneKeys[6] = { VK_S, VK_D, VK_F, VK_J, VK_K, VK_L };
}

void JudgeSystem::RegisterHit(int laneIndex, int rank)
{
	m_Combo++;
	if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;
	if (m_OnJudge) m_OnJudge(laneIndex, rank);
}

void JudgeSystem::ResetJudgeCounts()
{
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;
}

void JudgeSystem::MarkTapAutoMiss(std::vector<ChartNote>& notes, float musicTime)
{
	for (auto& note : notes)
	{
		if (note.judged) continue;
		if (!note.IsTap()) continue;
		if (musicTime - note.time > kGud)
		{
			note.judged = true;
			m_Miss++;
		}
	}
}

// ============================================================
// JudgeLane : 指定キーを「押した瞬間」に、そのレーンで
//             最も判定時刻が近いTAPノーツを判定する。
// ============================================================
void JudgeSystem::JudgeLane(int key, int laneIndex, std::vector<ChartNote>& notes, float musicTime, Sound& sound)
{
	if (!Input::GetKeyTrigger(key)) return; // 押した瞬間でなければ何もしない

	int bestIndex = -1;
	float bestDiff = 9999.0f;

	// 対象レーンのTAPノーツの中から、最も時間差が小さいものを探す
	for (int i = 0; i < (int)notes.size(); i++)
	{
		if (notes[i].judged) continue;
		if (notes[i].lane != laneIndex) continue;
		if (!notes[i].IsTap()) continue; // LONG/FLICKはここでは判定しない

		float diff = std::fabs(notes[i].time - musicTime);
		if (diff < bestDiff) { bestDiff = diff; bestIndex = i; }
	}
	if (bestIndex == -1)
	{
		if (m_OnEmptyPress) m_OnEmptyPress(laneIndex);
		return;
	}
	if (bestDiff > kGud) return; // 判定圏外なら何もしない

	// 時間差に応じてランクを決定
	int rank = (bestDiff < kPerfect) ? 0 : (bestDiff < kGreat) ? 1 : 2;
	if (rank == 0)
	{
		m_Perfect++;
		sound.Play(SOUND_METAL_1);
	}
	else if (rank == 1)
	{
		m_Great++;
		sound.Play(SOUND_METAL_1);
	}
	else if (rank == 2)
	{
		m_Good++;
		sound.Play(SOUND_METAL_1);
	}

	notes[bestIndex].judged = true;
	RegisterHit(laneIndex, rank);
}

// ============================================================
// JudgeFlickNotes : 横フリックの判定（時間ベース）。
//   毎フレーム呼び出し、判定ウィンドウ(±kGud)に入っている間、
//   プレイヤーが要求サイドにいれば時間差でランク判定して成立させる。
//   ウィンドウを過ぎてしまったらミス確定。
// ============================================================
void JudgeSystem::JudgeFlickNotes(std::vector<ChartNote>& notes, float musicTime, int playerSide)
{
	for (auto& n : notes)
	{
		if (n.judged) continue;
		if (!n.IsFlick()) continue;

		float diff = n.time - musicTime;    // 正：まだ来ていない　負：過ぎた
		int requiredSide = n.IsFlickRight() ? 1 : -1; // 右フリックなら1、左フリックなら-1

		if (fabsf(diff) <= kGud && playerSide == requiredSide)
		{
			float absDiff = fabsf(diff);
			int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;

			// レーン範囲（From〜To）を求めて、範囲内すべてのレーンに判定エフェクトを出す
			int laneFrom = n.lane;
			int laneTo = (n.flickLaneTo >= 0) ? n.flickLaneTo : n.lane;
			int lo = std::min(laneFrom, laneTo);
			int hi = std::max(laneFrom, laneTo);

			if (rank == 0) m_Perfect++;
			else if (rank == 1) m_Great++;
			else m_Good++;

			for (int L = lo; L <= hi; L++)
			{
				if (L < 0 || L >= 6) continue;
				RegisterHit(L, rank);
			}

			n.judged = true;
			continue;
		}

		// 判定ウィンドウを過ぎても成立しなかった場合はミス確定
		if (diff < -kGud)
		{
			n.judged = true;
			m_Miss++;
		}
	}
}

// ============================================================
// JudgeLongNoteHold : ロングノーツの押しっぱなし判定。
//   1. 先端が判定ラインに到達するタイミングでキーが押されていれば「開始」成立
//   2. 開始後は一定間隔(kLongTickInterval)ごとにtick加算（継続判定）
//   3. 終了時刻まで押しっぱなしならクリア、途中で離したら即ミス
// ============================================================
void JudgeSystem::JudgeLongNoteHold(std::vector<ChartNote>& notes, float musicTime)
{
	for (auto& n : notes)
	{
		if (!n.IsLong()) continue;
		if (n.judged) continue;

		if (n.lane < 0 || n.lane >= 6) continue;
		bool isDown = Input::GetKeyPress(kLaneKeys[n.lane]);

		// ── まだ開始していない場合 ──
		if (!n.longStarted && !n.longFailed)
		{
			float diff = n.arrivalTime - musicTime;
			if (isDown && fabsf(diff) <= kGud)
			{
				float absDiff = fabsf(diff);
				int rank = (absDiff < kPerfect) ? 0 : (absDiff < kGreat) ? 1 : 2;
				if (rank == 0) m_Perfect++;
				else if (rank == 1) m_Great++;
				else m_Good++;
				RegisterHit(n.lane, rank);

				n.longStarted = true;
				n.nextTickTime = n.arrivalTime + kLongTickInterval;
			}
			else if (diff < -kGud)
			{
				// 開始を逃した → Failed扱いにするだけ。ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}
		// ── 開始済み・まだ成功継続中 ──
		else if (n.longStarted && !n.longFailed)
		{
			if (isDown)
			{
				while (n.nextTickTime <= musicTime && n.nextTickTime < n.time)
				{
					m_Perfect++;
					n.nextTickTime += kLongTickInterval;
				}
			}
			else
			{
				// 離した瞬間 → Failedにするが、ノーツはendTimeまで残す
				n.longFailed = true;
				m_Miss++;
			}
		}

		// ── 終了時刻に到達したら、ここで初めて judged=true にする ──
		if (musicTime >= n.time)
		{
			if (!n.longFailed) m_Perfect++; // 最後まで成功していればクリア扱い
			n.judged = true;
		}
	}
}

//オート用　到達時刻を過ぎたらパーフェクトに確定させる
void JudgeSystem::UpdateAutoJudgge(std::vector<ChartNote>& notes, float musicTime, Sound& sound)
{
	for (auto& note : notes)
	{
		if (note.judged) continue;

		if (note.IsTap())
		{
			if (musicTime >= note.time)
			{
				m_Perfect++;
				sound.Play(SOUND_METAL_1);
				RegisterHit(note.lane, 0);
				note.judged = true;
			}
		}
		else if (note.IsFlick())
		{
			if (musicTime >= note.time)
			{
				int laneFrom = note.lane;
				int laneTo = (note.flickLaneTo >= 0) ? note.flickLaneTo : note.lane;
				int lo = std::min(laneFrom, laneTo);
				int hi = std::max(laneFrom, laneTo);

				m_Perfect++;
				for (int L = lo; L <= hi; L++)
				{
					if (L < 0 || L >= 6) continue;
					RegisterHit(L, 0);
				}
				note.judged = true;
			}
		}
		else if (note.IsLong())
		{
			// 先端到達で自動的に押し始めが成立
			if (!note.longStarted && musicTime >= note.arrivalTime)
			{
				m_Perfect++;
				sound.Play(SOUND_SUZU);
				RegisterHit(note.lane, 0);
				note.longStarted = true;
				note.nextTickTime = note.arrivalTime + kLongTickInterval;
			}

			// 押しっぱなし中の継続判定（オートなので常に成功）
			if (note.longStarted)
			{
				while (note.nextTickTime <= musicTime && note.nextTickTime < note.time)
				{
					m_Perfect++;
					sound.Play(SOUND_SUZU);
					note.nextTickTime += kLongTickInterval;
				}
			}

			// 終了時刻に到達したら確定（longFailedには絶対にならない）
			if (musicTime >= note.time)
			{
				if (note.longStarted) m_Perfect++;
				sound.Play(SOUND_LONG_END);
				note.judged = true;
			}
		}
	}
}
```

---

## 3. ChartObject.h の変更点

### 3-1. include追加

`#include "HomingBeamSystem.h"` の下に1行追加：

```cpp
#include "HomingBeamSystem.h"
#include "JudgeSystem.h"
```

### 3-2. 公開の判定カウント取得関数を委譲に変更

削除:
```cpp
	// 判定結果の参照用
	int GetPerfectCount() const { return m_Perfect; }
	int GetGreatCount()   const { return m_Great; }
	int GetGoodCount()    const { return m_Good; }
	int GetMissCount()    const { return m_Miss; }
	int GetMaxComboCount() const { return m_MaxCombo; }
```

追加（同じ場所に）:
```cpp
	// 判定結果の参照用
	int GetPerfectCount() const { return m_JudgeSystem.GetPerfectCount(); }
	int GetGreatCount()   const { return m_JudgeSystem.GetGreatCount(); }
	int GetGoodCount()    const { return m_JudgeSystem.GetGoodCount(); }
	int GetMissCount()    const { return m_JudgeSystem.GetMissCount(); }
	int GetMaxComboCount() const { return m_JudgeSystem.GetMaxComboCount(); }
```

### 3-3. UpdateAutoJudgge宣言を削除し、JudgeSystemメンバを追加

削除:
```cpp
	bool m_AutoMode = false;
	void UpdateAutoJudgge();	//autoモード
```

追加:
```cpp
	bool m_AutoMode = false;

	JudgeSystem m_JudgeSystem;
```
（`void UpdateAutoJudgge();` の行だけ消えて、代わりに `JudgeSystem m_JudgeSystem;` を足すイメージです）

### 3-4. JudgeLane/JudgeFlickNotes/JudgeLongNoteHold の宣言を削除

削除:
```cpp
	// ── 判定処理 ──
	void JudgeLane(int key, int laneIndex);	// TAPノーツ：キーを押した瞬間の判定
	void JudgeFlickNotes();					// FLICKノーツ：時間ウィンドウ内でサイド一致を判定
	void JudgeLongNoteHold();					// LONGNOTE：押し始め〜押しっぱなし〜離すを判定
```

代わりにコメントだけ残す（任意）:
```cpp
	// ── 判定処理は JudgeSystem に切り出し済み ──
```

### 3-5. OnJudgeの宣言はそのまま残す

`void OnJudge(int laneIndex, int rank);` の行は**削除しません**。中身（.cpp側）だけ後で変更します。

### 3-6. 判定カウント本体・判定タイミング定数を削除

削除:
```cpp
	int m_BeatsPerBar = 4;	// 拍子（メタデータから読み込み）
	int m_Combo = 0;		// 現在のコンボ数

	// ── 判定カウント ──
	int m_Perfect = 0;
	int m_Great = 0;
	int m_Good = 0;
	int m_Miss = 0;
	int m_MaxCombo = 0;

	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
	float m_FrameSize = 150.0f;			// 判定円の外枠の表示サイズ

	// ── 判定タイミングの許容誤差（秒）。値が小さいほどシビア ──
	static constexpr float kPerfect = 0.2f;
	static constexpr float kGreat = 0.4f;
	static constexpr float kGud = 0.6f;			// この範囲を過ぎたら自動ミス
	static constexpr float kLongTickInterval = 0.05f;	// ロングノーツの継続判定(tick)間隔
```

置き換え後（Perfect〜kLongTickIntervalの6項目を削除、他は残す）:
```cpp
	int m_BeatsPerBar = 4;	// 拍子（メタデータから読み込み）

	float m_ComboPopTimer = 0.0f;		// コンボ数字が拡大表示される残り時間
	float m_NuberSplite_Size = 80.0f;	// コンボ数字1桁の表示サイズ
	float m_FrameSize = 150.0f;			// 判定円の外枠の表示サイズ
```

**注意**: `m_Combo`, `m_Perfect`, `m_Great`, `m_Good`, `m_Miss`, `m_MaxCombo`, `kPerfect`, `kGreat`, `kGud`, `kLongTickInterval` の10個をまとめて消してください（JudgeSystem.hに同じものが入っています）。

以上で ChartObject.h は完了です（335行 → だいたい320行前後になるはずです。数自体はそこまで減りませんが、責務は減ります）。

---

## 4. ChartObject.cpp の変更点

### 4-1. Init() 内、判定カウント関連のリセットは元々ないので変更なし

（Init()には該当コードがないので何もしません）

### 4-2. LoadChart() のリセット処理

削除:
```cpp
	m_Perfect = 0;
	m_Great = 0;
	m_Good = 0;
	m_Miss = 0;
```

追加:
```cpp
	m_JudgeSystem.ResetJudgeCounts();
```

### 4-3. Update() 内、最初のTAP自動ミス判定ループ

削除:
```cpp
	// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	// LONG/FLICKは専用の判定関数(JudgeLongNoteHold/JudgeFlickNotes)側でミス処理する
	for (auto& note : m_Notes)
	{
		if (note.judged) continue;
		if (!note.IsTap()) continue;
		if (m_MusicTime - note.time > kGud)
		{
			note.judged = true;
			m_Miss++;
		}
	}

	// 接近音（判定時刻の1小節前になったら一度だけ再生）
```
（↑最後の「接近音」コメント行は消さないでください。目印として書いているだけです）

追加:
```cpp
	// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
	m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);

	// 接近音（判定時刻の1小節前になったら一度だけ再生）
```

### 4-4. Update() 内、if(m_AutoMode) 〜 else ブロック全体

削除:
```cpp
	if (m_AutoMode)
	{
		UpdateAutoJudgge();
	}
	else
	{
		// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス）
		// LONG/FLICKは専用の判定関数(JudgeLongNoteHold/JudgeFlickNotes)側でミス処理する
		for (auto& note : m_Notes)
		{
			if (note.judged) continue;
			if (!note.IsTap()) continue;
			if (m_MusicTime - note.time > kGud)
			{
				note.judged = true;
				m_Miss++;
			}
		}

		// S/D/F/J/K/Lキーで、TAPノーツのトリガー判定を行う
		for (int lane = 0; lane < 6; lane++)
		{
			JudgeLane(kLaneKeys[lane], lane);
		}

		// フリック・ロングノーツの判定
		JudgeFlickNotes();
		JudgeLongNoteHold();
	}
```

追加:
```cpp
	if (m_AutoMode)
	{
		m_JudgeSystem.UpdateAutoJudgge(m_Notes, m_MusicTime, sound);
	}
	else
	{
		// TAPノーツの自動ミス判定（判定圏外を過ぎたら強制ミス。上と同じ内容の重複だが元コードの動作を維持）
		m_JudgeSystem.MarkTapAutoMiss(m_Notes, m_MusicTime);

		// S/D/F/J/K/Lキーで、TAPノーツのトリガー判定を行う
		int playerSideForFlick = m_PlayerRef ? m_PlayerRef->GetSide() : 0;
		for (int lane = 0; lane < 6; lane++)
		{
			m_JudgeSystem.JudgeLane(kLaneKeys[lane], lane, m_Notes, m_MusicTime, sound);
		}

		// フリック・ロングノーツの判定
		m_JudgeSystem.JudgeFlickNotes(m_Notes, m_MusicTime, playerSideForFlick);
		m_JudgeSystem.JudgeLongNoteHold(m_Notes, m_MusicTime);
	}
```

### 4-5. コールバックの登録（Init()の先頭あたりに追加）

`void ChartObject::Init()` の中、一番最初の行（`m_AutoMode = Game::GetInstance()->IsAutoMode();` の直前か直後）に追加してください:

```cpp
	m_JudgeSystem.SetOnJudge([this](int laneIndex, int rank) { OnJudge(laneIndex, rank); });
	m_JudgeSystem.SetOnEmptyPress([this](int laneIndex) { OnEmptyLanePress(laneIndex); });
```

これでJudgeSystemがヒット/空振りを検出したときに、ChartObject側の演出処理を呼び出せるようになります。

### 4-6. JudgeLane/JudgeFlickNotes/JudgeLongNoteHold/UpdateAutoJudgge の関数本体を削除

以下の4関数はまるごと削除してください（中身はJudgeSystem.cppに移植済みです）:

- `void ChartObject::UpdateAutoJudgge() { ... }` （コメントブロック含めて全部）
- `void ChartObject::JudgeLane(int key, int laneIndex) { ... }`
- `void ChartObject::JudgeFlickNotes() { ... }`（前のコメントブロック含む）
- `void ChartObject::JudgeLongNoteHold() { ... }`（前のコメントブロック含む）

削除する範囲の目印:

- `UpdateAutoJudgge` は `//オート用　到達時刻を過ぎたらパーフェクトに確定させる`（`//`の直後にスペース無し、間の空白は全角スペース）というコメント行から、閉じ `}` まで。次の行は空行を挟んで `// EvaluateNotePosition / EvaluateLongNotePosition は NotePath.h/.cpp に切り出し済み` のコメントです（これは消さないでください）。

- `JudgeLane` は `// ============================================================` `// JudgeLane : 指定キーを「押した瞬間」に、そのレーンで` から始まるコメントブロックごと、対応する `}` まで。

- `JudgeFlickNotes` は `// JudgeFlickNotes : 横フリックの判定（時間ベース）。` のコメントブロックごと、対応する `}` まで。

- `JudgeLongNoteHold` は `// JudgeLongNoteHold : ロングノーツの押しっぱなし判定。` のコメントブロックごと、対応する `}` まで。次の行に `// InitBeamScatter は ChartLoader.cpp のローカル関数に切り出し済み` というコメントがありますが、これは消さないでください。

置き換え後、この位置には次のコメント1行だけ残すことをお勧めします:
```cpp
// JudgeLane / JudgeFlickNotes / JudgeLongNoteHold / UpdateAutoJudgge は JudgeSystem に切り出し済み
```

### 4-7. OnJudge() の中身を修正（コンボ加算部分を削除）

`void ChartObject::OnJudge(int laneIndex, int rank)` の中身、冒頭付近を変更します。

削除:
```cpp
	m_Combo++;

	if (laneIndex >= 0 && laneIndex < 6)
	{
		m_BarrierEffects[laneIndex].active = true;
		m_BarrierEffects[laneIndex].timer = 0.0f;

		m_JudgeRankEffects[laneIndex].active = true;
		m_JudgeRankEffects[laneIndex].rank = rank;
		m_JudgeRankEffects[laneIndex].timer = kJudgeRankPopDuration;
	}

	if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;
	m_ComboPopTimer = 0.15f; // コンボ数字が一瞬拡大表示される演出用タイマー
```

追加:
```cpp
	// m_Combo++ / m_MaxCombo更新は JudgeSystem::RegisterHit に移動済み

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

（変更点は「m_Combo++;」の行と「if (m_Combo > m_MaxCombo) m_MaxCombo = m_Combo;」の行の2行を消しただけです。残りはそのままです。）

### 4-8. 空振り時の処理を新しい関数として追加

`OnJudge` のすぐ後（同じファイル内、`void ChartObject::UpdateLightEffects` の前あたり）に新しい関数を追加してください。これは元々 `JudgeLane` の中にあった「対象ノーツが無かった場合」のライトエフェクト処理を、そのまま関数として独立させたものです。

追加（新しい関数まるごと）:
```cpp
// ============================================================
// OnEmptyLanePress : そのレーンに判定対象のTAPノーツが無いときに
//   空押ししてしまった場合のフック（元々JudgeLane内にあった処理）。
// ============================================================
void ChartObject::OnEmptyLanePress(int laneIndex)
{
	ChartKeyLightEffect le;
	le.lane = laneIndex;
	le.startTime = m_MusicTime;
	le.duration = 0.6f;
	le.intensity = 2.2f;
	le.origin = m_JudgeCircles[laneIndex].GetPosition(m_MusicTime);

	le.origin.x += (laneIndex < 3) ? -kLaneOffsetFix[laneIndex] : kLaneOffsetFix[laneIndex];

	m_LightEffects.push_back(le);
}
```

これに対応する宣言もChartObject.hに追加してください。`void OnJudge(int laneIndex, int rank);` の下に1行追加:

```cpp
	void OnJudge(int laneIndex, int rank); // rank: 0=PERFECT; 1=GREAT 2=GOOD
	void OnEmptyLanePress(int laneIndex);  // 空押し時のライトエフェクト（元JudgeLane内の処理）
```

### 4-9. Draw() 内、コンボ数取得部分

ChartObject.cppのDraw()内の `int combo = m_Combo;` を探して変更します（JudgeSystem.h側の `GetComboCount()` は上の1章で追加済みの前提です）。

削除:
```cpp
		int combo = m_Combo;
```

追加:
```cpp
		int combo = m_JudgeSystem.GetComboCount();
```

---

## 5. Visual Studioへの新規ファイル追加について

`JudgeSystem.h` / `JudgeSystem.cpp` は、ソリューションエクスプローラーで「追加 → 新しい項目」または「既存の項目」でプロジェクトに追加すれば、`golf.vcxproj` / `golf.vcxproj.filters` への登録は Visual Studio が自動でやってくれます（手動でXMLを編集する必要はありません）。フィルターは「ソースファイル\Chart」「ヘッダーファイル\Chart」に入れておくと、既存のChart関連ファイルと揃います。

---

## 6. 期待される削減量

- ChartObject.cpp から JudgeLane・JudgeFlickNotes・JudgeLongNoteHold・UpdateAutoJudgge の中身（合計約260〜300行）が無くなります。
- ChartObject.h から判定カウント6個＋定数4個（合計10個の宣言）が無くなります。
- 新規に JudgeSystem.h（約45行）・JudgeSystem.cpp（約230行）ができます。

ビルドしてエラーが出た場合は、いつも通りスクリーンショットを送ってください。
