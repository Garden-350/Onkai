#pragma once
// ============================================================
// NotePalette : ノーツの種類を「絵」で選ぶ（クリップスタジオのツールパレットのように）
//   ・ノーツの種類の表は NotePalette.cpp の kNoteKinds だけ（色・名前・絵・ショートカット）
//   ・絵は asset/icons/notes/<id>.png。写真やスクショに差し替えたいときは、同じ名前で置き換えるだけ
//   ・2D の譜面に置く絵は asset/notes2d/<sprite>.png（tap.png / trace.png …）
//   ・新しいノーツ：Note.h の NoteType に足す → kNoteKinds に 1 行 → 絵を置く
// ============================================================
#include "Note.h"
#include "imgui.h"

namespace NotePalette
{
	// 操作として登録する（ツールバー・「ノーツ」メニュー・数字キー 1〜）。current は今選んでいる種類
	void Register(NoteType* current);
	// 「ノーツ」の窓（大きい絵 / 小さい絵＋名前 / 文字だけ を切り替えられる）
	void DrawPanel();

	// 2D の譜面にノーツを 1 個描く。center は判定ラインに届く時刻・レーンの真ん中。絵は asset/notes2d/
	void DrawNote2D(ImDrawList* draw, NoteType t, ImVec2 center, float width, float height, bool selected);

	ImU32 Color(NoteType t);			// 2D の譜面で使う色
	const char* Name(NoteType t);		// 英語の名前（一覧などで使う。今までの NoteTypeName と同じ）
	const char* Label(NoteType t);		// 画面に出す日本語の名前
}
