#include "pch.h"
#include "NotePalette.h"
#include "EditorShell.h"
#include <string>

namespace
{
	// ============================================================
	// ノーツの種類の表（新しいノーツはここに 1 行足す）
	//   id    : 中の名前。絵のファイル名にもなる（asset/icons/notes/<id>.png）
	//   name  : 英語の名前（一覧の表示・今までの NoteTypeName）
	//   label : 画面に出す名前
	//   color : 2D の譜面の色
	//   tip   : マウスを乗せたときの説明
	//   key   : ショートカット（数字キー）
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
	};
	const NoteKind kNoteKinds[] =
	{
		{ TAP,         "tap",         "TAP",         u8"タップ",     IM_COL32(90, 255, 150, 230),  u8"叩く",                         ImGuiKey_1 },
		{ LONGNOTE,    "long",        "LONG",        u8"ロング",     IM_COL32(90, 255, 150, 230),  u8"押しっぱなし",                 ImGuiKey_2 },
		{ FLICK_RIGHT, "flick_right", "FLICK_RIGHT", u8"フリック右", IM_COL32(255, 140, 220, 230), u8"右へはじく",                   ImGuiKey_3 },
		{ FLICK_LEFT,  "flick_left",  "FLICK_LEFT",  u8"フリック左", IM_COL32(255, 140, 220, 230), u8"左へはじく",                   ImGuiKey_4 },
		{ TRACE,       "trace",       "TRACE",       u8"トレース",   IM_COL32(255, 240, 120, 230), u8"判定の時刻に押していれば取れる", ImGuiKey_5 },
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

	ImU32 Color(NoteType t) { const NoteKind* k = Find(t); return k ? k->color : IM_COL32(255, 255, 255, 230); }
	const char* Name(NoteType t) { const NoteKind* k = Find(t); return k ? k->name : "Unknown"; }
	const char* Label(NoteType t) { const NoteKind* k = Find(t); return k ? k->label : u8"?"; }
}
