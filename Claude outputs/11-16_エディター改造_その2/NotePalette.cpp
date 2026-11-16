#include "pch.h"
#include "NotePalette.h"
#include "EditorShell.h"
#include <string>

namespace
{
	// ============================================================
	// ノーツの種類の表（新しいノーツはここに 1 行足す）
	//   id     : 中の名前。パレットの絵のファイル名にもなる（asset/icons/notes/<id>.png）
	//   name   : 英語の名前（一覧の表示・今までの NoteTypeName）
	//   label  : 画面に出す名前
	//   color  : 2D の譜面の色（ノーツの縁・ロングの胴）
	//   tip    : マウスを乗せたときの説明
	//   key    : ショートカット（数字キー）
	//   sprite : 2D の譜面に置く絵（asset/notes2d/<sprite>.png）。nullptr なら色の四角
	//   uvTop / uvBottom : 絵の縦のどこからどこまでを使うか（0〜1。真ん中の帯だけ使うと横長のノーツに合う）
	//   heightScale : ノーツの高さの倍率（絵が細かい物は少し高く）
	//   tint   : 絵に掛ける色（白＝そのまま）
	// ============================================================
	struct NoteKind
	{
		NoteType type;
		const char* id;
		const char* name;
		const char* label;
		ImU32 color;
		const char* tip;
		ImGuiKey key;
		const char* sprite;
		float uvTop, uvBottom;
		float heightScale;
		ImU32 tint;
	};
	const NoteKind kNoteKinds[] =
	{
		{ TAP,         "tap",         "TAP",         u8"タップ",     IM_COL32(90, 255, 150, 230),  u8"叩く",                         ImGuiKey_1,
		  "tap",   0.25f, 0.75f, 1.0f, IM_COL32(255, 255, 255, 255) },
		{ LONGNOTE,    "long",        "LONG",        u8"ロング",     IM_COL32(90, 255, 150, 230),  u8"押しっぱなし",                 ImGuiKey_2,
		  "tap",   0.25f, 0.75f, 1.0f, IM_COL32(200, 255, 230, 255) },
		{ FLICK_RIGHT, "flick_right", "FLICK_RIGHT", u8"フリック右", IM_COL32(255, 140, 220, 230), u8"右へはじく",                   ImGuiKey_3,
		  "tap",   0.25f, 0.75f, 1.0f, IM_COL32(255, 190, 235, 255) },
		{ FLICK_LEFT,  "flick_left",  "FLICK_LEFT",  u8"フリック左", IM_COL32(255, 140, 220, 230), u8"左へはじく",                   ImGuiKey_4,
		  "tap",   0.25f, 0.75f, 1.0f, IM_COL32(255, 190, 235, 255) },
		{ TRACE,       "trace",       "TRACE",       u8"トレース",   IM_COL32(255, 240, 120, 230), u8"判定の時刻に押していれば取れる", ImGuiKey_5,
		  "trace", 0.30f, 0.70f, 1.35f, IM_COL32(255, 250, 210, 255) },
	};

	NoteType* s_Current = nullptr;
	int s_ViewMode = 0;		// 0 大きい絵 / 1 小さい絵＋名前 / 2 文字だけ

	const NoteKind* Find(NoteType t)
	{
		for (const auto& k : kNoteKinds) if (k.type == t) return &k;
		return nullptr;
	}
	std::string IconName(const NoteKind& k) { return std::string("notes/") + k.id; }
	bool IsSelected(const NoteKind& k) { return s_Current && *s_Current == k.type; }
	void Select(const NoteKind& k) { if (s_Current) *s_Current = k.type; }

	// 大きい絵のマス（絵が無ければ色の四角）
	void DrawTile(const NoteKind& k, float size)
	{
		ImGui::PushID(k.id);
		ImGui::BeginGroup();
		ImVec2 p = ImGui::GetCursorScreenPos();
		bool selected = IsSelected(k);
		if (ImTextureID tex = EditorShell::Icon(IconName(k)))
		{
			if (ImGui::ImageButton("##tile", tex, ImVec2(size, size))) Select(k);
		}
		else
		{
			ImVec4 col = ImGui::ColorConvertU32ToFloat4(k.color);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x * 0.6f, col.y * 0.6f, col.z * 0.6f, 1.0f));
			if (ImGui::Button(k.name, ImVec2(size + 8.0f, size + 8.0f))) Select(k);
			ImGui::PopStyleColor();
		}
		if (selected)	// 選んでいるものは青い枠
		{
			ImVec2 q(p.x + ImGui::GetItemRectSize().x, p.y + ImGui::GetItemRectSize().y);
			ImGui::GetWindowDrawList()->AddRect(p, q, ImGui::GetColorU32(ImGuiCol_ButtonActive), 4.0f, 0, 3.0f);
		}
		ImGui::SetItemTooltip("%s\n%s", k.label, k.tip);
		// 名前（マスの真ん中にそろえる）
		float w = ImGui::GetItemRectSize().x;
		float tw = ImGui::CalcTextSize(k.label).x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - tw) * 0.5f));
		ImGui::TextUnformatted(k.label);
		ImGui::EndGroup();
		ImGui::PopID();
	}
}

namespace NotePalette
{
	void Register(NoteType* current)
	{
		s_Current = current;
		bool first = true;
		for (const auto& k : kNoteKinds)
		{
			EditorCommand c;
			c.id = std::string("note.") + k.id;
			c.label = k.label;
			c.icon = IconName(k);
			c.menu = u8"ノーツ";
			c.key = k.key;
			c.toolbar = true;
			c.separatorBefore = first;
			const NoteKind* kind = &k;
			c.run = [kind]() { Select(*kind); };
			c.isOn = [kind]() { return IsSelected(*kind); };
			EditorShell::AddCommand(c);
			first = false;
		}
	}

	void DrawPanel()
	{
		if (EditorShell::BeginPanel(u8"ノーツ"))
		{
			ImGui::RadioButton(u8"大きい絵", &s_ViewMode, 0); ImGui::SameLine();
			ImGui::RadioButton(u8"絵＋名前", &s_ViewMode, 1); ImGui::SameLine();
			ImGui::RadioButton(u8"文字", &s_ViewMode, 2);
			ImGui::Separator();

			if (s_ViewMode == 0)
			{
				// 窓の幅に合わせて、横に並べられるだけ並べる
				const float tile = 64.0f;
				float avail = ImGui::GetContentRegionAvail().x;
				float cell = tile + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
				int perRow = std::max(1, (int)(avail / cell));
				int i = 0;
				for (const auto& k : kNoteKinds)
				{
					if (i % perRow != 0) ImGui::SameLine();
					DrawTile(k, tile);
					i++;
				}
			}
			else
			{
				for (const auto& k : kNoteKinds)
				{
					ImGui::PushID(k.id);
					if (s_ViewMode == 1)
					{
						if (ImTextureID tex = EditorShell::Icon(IconName(k)))
						{
							ImGui::Image(tex, ImVec2(28, 28));
							ImGui::SameLine();
						}
						else
						{
							// 絵が無いときは色の丸
							ImVec2 p = ImGui::GetCursorScreenPos();
							ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 14, p.y + 14), 10.0f, k.color);
							ImGui::Dummy(ImVec2(28, 28));
							ImGui::SameLine();
						}
					}
					if (ImGui::Selectable(k.label, IsSelected(k), 0, ImVec2(0, s_ViewMode == 1 ? 28.0f : 0.0f)))
						Select(k);
					ImGui::SetItemTooltip("%s", k.tip);
					ImGui::PopID();
				}
			}
		}
		EditorShell::EndPanel();
	}

	// ============================================================
	// 2D の譜面にノーツを 1 個描く（判定ラインに届く時刻の所）
	// ============================================================
	void DrawNote2D(ImDrawList* draw, NoteType t, ImVec2 center, float width, float height, bool selected)
	{
		const NoteKind* k = Find(t);
		const ImU32 kindCol = k ? k->color : IM_COL32(255, 255, 255, 230);
		const float hw = width * 0.5f;
		const float hh = height * (k ? k->heightScale : 1.0f) * 0.5f;
		const ImVec2 a(center.x - hw, center.y - hh), b(center.x + hw, center.y + hh);

		ImTextureID tex = (k && k->sprite) ? EditorShell::Texture(std::string("asset/notes2d/") + k->sprite + ".png") : nullptr;
		if (tex)
			draw->AddImageRounded(tex, a, b, ImVec2(0.05f, k->uvTop), ImVec2(0.95f, k->uvBottom), k->tint, 3.0f);	// 絵の外枠は使わない
		else
			draw->AddRectFilled(a, b, (kindCol & 0x00FFFFFF) | 0xC8000000, 3.0f);	// 絵が無いときは色の四角

		// 種類の色の縁（同じ絵を使っていても、種類が分かるように）
		draw->AddRect(a, b, kindCol, 3.0f, 0, 2.0f);

		// フリックは向きの矢印
		if (t == FLICK_RIGHT || t == FLICK_LEFT)
		{
			float s = (t == FLICK_RIGHT) ? 1.0f : -1.0f;
			float tip = center.x + s * (hw * 0.55f + hh * 0.8f);
			float back = center.x + s * (hw * 0.55f - hh * 0.6f);
			draw->AddTriangleFilled(ImVec2(tip, center.y), ImVec2(back, center.y - hh * 1.3f), ImVec2(back, center.y + hh * 1.3f),
				IM_COL32(255, 255, 255, 235));
		}

		// 選んでいるノーツは黄色い枠
		if (selected)
		{
			draw->AddRect(ImVec2(a.x - 3.0f, a.y - 3.0f), ImVec2(b.x + 3.0f, b.y + 3.0f), IM_COL32(255, 220, 80, 255), 4.0f, 0, 3.0f);
		}
	}

	ImU32 Color(NoteType t) { const NoteKind* k = Find(t); return k ? k->color : IM_COL32(255, 255, 255, 230); }
	const char* Name(NoteType t) { const NoteKind* k = Find(t); return k ? k->name : "Unknown"; }
	const char* Label(NoteType t) { const NoteKind* k = Find(t); return k ? k->label : u8"?"; }
}
