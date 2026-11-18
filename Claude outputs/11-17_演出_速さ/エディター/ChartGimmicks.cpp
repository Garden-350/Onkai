#include "pch.h"
#include "ChartGimmicks.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using nlohmann::json;

namespace
{
	// ---- 種類（ボタン）：どれも type "speed"。速さの最初の値と色が違うだけ ----
	struct Kind
	{
		const char* label;		// ボタンの名前
		float speed;			// 置いた時の速さ
		ImU32 color;			// 帯の色
	};
	const Kind kKinds[] =
	{
		{ u8"逆走",     -1.0f, IM_COL32(235, 110, 100, 255) },
		{ u8"低速",      0.5f, IM_COL32(240, 190,  90, 255) },
		{ u8"止める",    0.0f, IM_COL32(170, 170, 185, 255) },
		{ u8"速くする",  2.0f, IM_COL32(110, 200, 240, 255) },
	};
	constexpr int kKindCount = (int)(sizeof(kKinds) / sizeof(kKinds[0]));

	// 速さの数字から、どの種類に見えるか
	int KindOf(const EditorGimmick& g)
	{
		if (g.speed < -0.001f) return 0;	// 逆走
		if (g.speed < 0.001f) return 2;		// 止める
		if (g.speed < 0.999f) return 1;		// 低速
		return 3;							// 速くする（1 ちょうども、ここ）
	}
	ImU32 WithAlpha(ImU32 c, int a) { return (c & 0x00FFFFFF) | ((ImU32)a << 24); }

	// ---- 状態 ----
	std::vector<EditorGimmick> g_Items;
	int g_Selected = -1;

	int g_PlaceKind = -1;					// 置く種類（-1 = 置かない）。ボタンで選ぶ
	std::string g_Message;					// 窓の下に出す一言（重なっている など）

	// 演出ノーツのドラッグ：頭をつかむと丸ごと動く、尾をつかむと長さが変わる（ロングと同じ）
	enum class Drag { None, Move, Tail };
	Drag g_Drag = Drag::None;
	int g_DragIndex = -1;
	float g_DragGrab = 0.0f;				// つかんだ所と頭の時刻の差（丸ごと動かす時）

	constexpr float kHeadH = 14.0f;			// 頭（ノーツの形）の高さ px

	// ---- 時間 ----
	float BeatSec(const EditorTimeInfo& t) { return (t.bpm > 0.0f) ? 60.0f / t.bpm : 0.5f; }

	// 「12 小節 3 拍」の形にする（1 小節目の 1 拍目 = offset）
	std::string BarBeat(const EditorTimeInfo& t, float sec)
	{
		int bpmBar = std::max(1, t.beatsPerMeasure);
		float beats = (sec - t.offset) / BeatSec(t);
		int bar = (int)std::floor(beats / bpmBar);
		float beatIn = beats - (float)(bar * bpmBar) + 1.0f;
		char buf[64];
		if (std::fabs(beatIn - std::round(beatIn)) < 0.01f)
			sprintf_s(buf, u8"%d小節 %d拍", bar + 1, (int)std::round(beatIn));
		else
			sprintf_s(buf, u8"%d小節 %.2f拍", bar + 1, beatIn);
		return buf;
	}

	// 同じ「速さ」の演出と重なるか（skip は自分）
	bool Overlaps(float a, float b, int skip)
	{
		for (int i = 0; i < (int)g_Items.size(); i++)
		{
			if (i == skip) continue;
			const auto& g = g_Items[i];
			if (a < g.end - 0.0001f && b > g.start + 0.0001f) return true;
		}
		return false;
	}

	void SortKeepSelection()
	{
		EditorGimmick sel;
		bool has = (g_Selected >= 0 && g_Selected < (int)g_Items.size());
		if (has) sel = g_Items[g_Selected];
		std::sort(g_Items.begin(), g_Items.end(), [](const EditorGimmick& a, const EditorGimmick& b) { return a.start < b.start; });
		g_Selected = -1;
		if (has)
			for (int i = 0; i < (int)g_Items.size(); i++)
				if (g_Items[i].start == sel.start && g_Items[i].end == sel.end) { g_Selected = i; break; }
	}

	// 逆走・止める の中にあるノーツの数
	int NotesInside(const EditorGimmick& g, const std::vector<float>& noteTimes)
	{
		int n = 0;
		for (float t : noteTimes) if (t > g.start + 0.0001f && t < g.end - 0.0001f) n++;
		return n;
	}
}

namespace ChartGimmicks
{
	std::vector<EditorGimmick>& Items() { return g_Items; }
	bool CapturesMouse() { return g_DragIndex >= 0; }

	bool SelectedStart(float* outStart)
	{
		if (g_Selected < 0 || g_Selected >= (int)g_Items.size()) return false;
		if (outStart) *outStart = g_Items[g_Selected].start;
		return true;
	}

	// ============================================================
	// 保存・読み込み
	// ============================================================
	void OnLoad(const json& j)
	{
		g_Items.clear();
		g_Selected = -1;
		g_Drag = Drag::None;
		g_DragIndex = -1;
		g_Message.clear();
		if (!j.contains("gimmicks") || !j["gimmicks"].is_array()) return;
		for (const auto& e : j["gimmicks"])
		{
			if (!e.is_object()) continue;
			EditorGimmick g;
			g.type = e.value("type", std::string("speed"));
			if (g.type != "speed") continue;	// まだ知らない種類（今は速さだけ）
			g.start = e.value("start", 0.0f);
			g.end = e.value("end", 0.0f);
			g.easeIn = e.value("easeIn", 0.5f);
			g.easeOut = e.value("easeOut", 0.5f);
			g.speed = e.value("speed", 1.0f);
			if (g.end > g.start) g_Items.push_back(g);
		}
		SortKeepSelection();
	}

	void Write(std::ostream& file)
	{
		if (g_Items.empty()) return;
		file << ",\n\"gimmicks\":[\n";
		for (size_t i = 0; i < g_Items.size(); i++)
		{
			const auto& g = g_Items[i];
			json e = { { "type", g.type }, { "start", g.start }, { "end", g.end },
				{ "easeIn", g.easeIn }, { "easeOut", g.easeOut }, { "speed", g.speed } };
			file << e.dump() << (i + 1 < g_Items.size() ? ",\n" : "\n");
		}
		file << "]";
	}

	// ============================================================
	// 「演出」の窓
	// ============================================================
	void DrawPanel(const EditorTimeInfo& t, const std::vector<float>& noteTimes,
		const std::string& chartName, const std::filesystem::path& gameRoot, const std::function<bool()>& saveToGame)
	{
		if (!ImGui::Begin(u8"演出###Gimmicks"))
		{
			ImGui::End();
			return;
		}

		// Esc で、置くのをやめる
		if (g_PlaceKind >= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) g_PlaceKind = -1;

		// ---- 種類のボタン ----
		ImGui::SeparatorText(u8"置く（種類を選んで、譜面の右の列をクリック）");
		for (int k = 0; k < kKindCount; k++)
		{
			if (k > 0) ImGui::SameLine();
			ImVec4 c = ImGui::ColorConvertU32ToFloat4(kKinds[k].color);
			bool active = (g_PlaceKind == k);
			ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(c.x, c.y, c.z, 1.0f) : ImVec4(c.x * 0.45f, c.y * 0.45f, c.z * 0.45f, 0.9f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x * 0.75f, c.y * 0.75f, c.z * 0.75f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : ImVec4(1, 1, 1, 1));
			if (ImGui::Button(kKinds[k].label, ImVec2(70, 0)))
			{
				g_PlaceKind = active ? -1 : k;	// もう一度押すとやめる
				g_Message.clear();
			}
			ImGui::PopStyleColor(3);
		}
		if (g_PlaceKind >= 0)
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.4f, 1.0f), u8"＞ 列をクリックすると【%s】の演出ノーツ（1 小節）を置きます（Esc でやめる）", kKinds[g_PlaceKind].label);
		else
			ImGui::TextDisabled(u8"頭をドラッグ：丸ごと動かす　尾をドラッグ：長さ　右クリック：消す");
		ImGui::TextDisabled(u8"演出ノーツが判定ラインを通った所で、演出が始まります（本番では見えません）");
		if (!g_Message.empty())
			ImGui::TextDisabled("%s", g_Message.c_str());

		// ---- 一覧 ----
		ImGui::SeparatorText(u8"一覧");
		if (g_Items.empty())
			ImGui::TextDisabled(u8"まだありません");
		for (int i = 0; i < (int)g_Items.size(); i++)
		{
			const auto& g = g_Items[i];
			int k = KindOf(g);
			char row[160];
			sprintf_s(row, u8"%s  %s 〜 %s  （×%.2f）###gim%d", kKinds[k].label,
				BarBeat(t, g.start).c_str(), BarBeat(t, g.end).c_str(), g.speed, i);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kKinds[k].color));
			if (ImGui::Selectable(row, g_Selected == i)) g_Selected = (g_Selected == i) ? -1 : i;
			ImGui::PopStyleColor();
		}

		// ---- 選んでいる演出の設定 ----
		if (g_Selected >= 0 && g_Selected < (int)g_Items.size())
		{
			EditorGimmick& g = g_Items[g_Selected];
			ImGui::SeparatorText(u8"選んでいる演出");
			ImGui::PushItemWidth(150);

			float speed = g.speed;
			if (ImGui::DragFloat(u8"速さ", &speed, 0.01f, -3.0f, 3.0f, "%.2f"))
				g.speed = std::clamp(speed, -3.0f, 3.0f);
			ImGui::SameLine();
			ImGui::TextDisabled(u8"（1=普通 0=止まる マイナス=逆走）");

			float spb = BeatSec(t);
			float lenBeats = (g.end - g.start) / spb;
			ImGui::Text(u8"始まり %s　長さ %.2f 拍", BarBeat(t, g.start).c_str(), lenBeats);

			float sBeat = (g.start - t.offset) / spb;
			if (ImGui::DragFloat(u8"始まり（拍）", &sBeat, 0.25f, 0.0f, 100000.0f, "%.2f"))
			{
				float ns = t.offset + sBeat * spb;
				float len = g.end - g.start;
				if (!Overlaps(ns, ns + len, g_Selected) && ns >= 0.0f) { g.start = ns; g.end = ns + len; }
			}
			if (ImGui::DragFloat(u8"長さ（拍）", &lenBeats, 0.25f, 0.25f, 10000.0f, "%.2f"))
			{
				float ne = g.start + std::max(0.25f, lenBeats) * spb;
				if (!Overlaps(g.start, ne, g_Selected)) g.end = ne;
			}
			ImGui::DragFloat(u8"入り（拍）", &g.easeIn, 0.05f, 0.0f, 16.0f, "%.2f");
			ImGui::DragFloat(u8"抜け（拍）", &g.easeOut, 0.05f, 0.0f, 16.0f, "%.2f");
			g.easeIn = std::clamp(g.easeIn, 0.0f, 16.0f);
			g.easeOut = std::clamp(g.easeOut, 0.0f, 16.0f);
			ImGui::PopItemWidth();

			// 逆走：どれくらい先が下から見えるか（入り・抜けの分はざっくり半分にする）
			if (g.speed < 0.0f)
			{
				float sec = std::fabs(g.speed) * std::max(0.0f, (g.end - g.start) - 0.5f * (g.easeIn + g.easeOut) * spb);
				ImGui::Text(u8"この先 約 %.1f 拍ぶんの譜面が、判定円の下から上がってくる", sec / spb);
			}
			// 逆走・止める の中のノーツ
			if (g.speed <= 0.001f)
			{
				int n = NotesInside(g, noteTimes);
				if (n > 0)
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), u8"！ この区間の中にノーツが %d 個あります（読みにくくなります）", n);
			}

			if (ImGui::Button(u8"＞ この演出を確認（F5）"))
			{
				EditorTimeInfo tt = t;
				tt.now = g.start;
				ChartExtras::PlayFromHere(tt, chartName, gameRoot, saveToGame);
			}
			ImGui::SameLine();
			if (ImGui::Button(u8"消す"))
			{
				g_Items.erase(g_Items.begin() + g_Selected);
				g_Selected = -1;
			}
			ImGui::SameLine();
			if (ImGui::Button(u8"選ぶのをやめる")) g_Selected = -1;
			ImGui::TextDisabled(u8"選んでいる間の F5 は、この演出の小節から始まります");
		}

		ImGui::End();
	}

	// ============================================================
	// 譜面の上：演出ノーツ（頭 = 始まり、尾 = 終わり）を描く・置く・選ぶ・動かす
	// ============================================================
	bool DrawTimeline(const TimelineView& v, const EditorTimeInfo& t)
	{
		if (!v.draw || !v.timeToY || !v.yToTime) return false;
		ImDrawList* draw = v.draw;
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool inCol = v.hovered && mouse.x >= v.colX0 && mouse.x <= v.colX1 && mouse.y >= v.yTop && mouse.y <= v.yBottom;
		auto snap = [&](float s) { return v.snap ? v.snap(s) : s; };
		const float spb = BeatSec(t);
		const float minLen = spb * 0.25f;
		const float cx = (v.colX0 + v.colX1) * 0.5f;
		const float headHalfW = (v.colX1 - v.colX0) * 0.5f - 3.0f;	// 頭の半分の幅
		const float tailHalfW = headHalfW * 0.45f;						// 尾（胴）の半分の幅

		// ---- 列の背景 ----
		draw->AddRectFilled(ImVec2(v.colX0, v.yTop), ImVec2(v.colX1, v.yBottom), IM_COL32(26, 24, 34, 255));
		draw->AddLine(ImVec2(v.colX0, v.yTop), ImVec2(v.colX0, v.yBottom), IM_COL32(80, 70, 100, 160));

		// ---- 演出ノーツ ----
		int hoverIndex = -1;
		Drag hoverPart = Drag::None;
		for (int i = 0; i < (int)g_Items.size(); i++)
		{
			const auto& g = g_Items[i];
			float yS = v.timeToY(g.start), yE = v.timeToY(g.end);	// 時間が上へ進むので yE < yS
			if (yS + kHeadH < v.yTop || yE - 4.0f > v.yBottom) continue;
			ImU32 c = kKinds[KindOf(g)].color;
			bool sel = (i == g_Selected);

			// 尾（胴）：入り・抜けの所は薄く
			float yIn = v.timeToY(std::min(g.end, g.start + g.easeIn * spb));
			float yOut = v.timeToY(std::max(g.start, g.end - g.easeOut * spb));
			draw->AddRectFilled(ImVec2(cx - tailHalfW, yE), ImVec2(cx + tailHalfW, yS), WithAlpha(c, sel ? 110 : 70));
			if (yIn > yOut) draw->AddRectFilled(ImVec2(cx - tailHalfW, yOut), ImVec2(cx + tailHalfW, yIn), WithAlpha(c, sel ? 200 : 150));
			// 尾の端（終わり）
			draw->AddRectFilled(ImVec2(cx - headHalfW * 0.8f, yE - 2.0f), ImVec2(cx + headHalfW * 0.8f, yE + 2.0f), WithAlpha(c, 230), 1.0f);

			// 頭（ノーツの形）：判定ラインの所に来たら演出が始まる
			ImVec2 hA(cx - headHalfW, yS - kHeadH * 0.5f), hB(cx + headHalfW, yS + kHeadH * 0.5f);
			draw->AddRectFilled(hA, hB, WithAlpha(c, 255), 4.0f);
			draw->AddRect(hA, hB, sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 26, 200), 4.0f, 0, sel ? 2.0f : 1.0f);
			const char* label = kKinds[KindOf(g)].label;
			char first[8] = {};
			int len = ((unsigned char)label[0] >= 0xE0) ? 3 : 1;	// UTF-8 の 1 文字
			memcpy(first, label, len);
			ImGui::SetWindowFontScale(0.75f);
			ImVec2 ts = ImGui::CalcTextSize(first);
			draw->AddText(ImVec2(cx - ts.x * 0.5f, yS - ts.y * 0.5f), IM_COL32(15, 15, 20, 255), first);
			ImGui::SetWindowFontScale(1.0f);

			// 選んでいる演出：始まりの所に、レーンをまたぐ線（ここを判定ラインが通ると始まる）と、区間のうっすらした色
			if (sel)
			{
				draw->AddRectFilled(ImVec2(v.laneX0, yE), ImVec2(v.laneX1, yS), WithAlpha(c, 18));
				draw->AddLine(ImVec2(v.laneX0, yS), ImVec2(v.laneX1, yS), WithAlpha(c, 200), 1.5f);
			}

			// マウスが頭・尾の端・胴のどこか（頭を一番優先）
			if (inCol)
			{
				if (std::fabs(mouse.y - yS) <= kHeadH * 0.5f + 2.0f) { hoverIndex = i; hoverPart = Drag::Move; }
				else if (hoverPart != Drag::Move && std::fabs(mouse.y - yE) <= 5.0f) { hoverIndex = i; hoverPart = Drag::Tail; }
				else if (hoverIndex < 0 && mouse.y <= yS && mouse.y >= yE) { hoverIndex = i; hoverPart = Drag::None; }	// 胴：選ぶだけ
			}
		}

		// ---- ドラッグ中 ----
		if (g_DragIndex >= 0)
		{
			if (g_DragIndex < (int)g_Items.size() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				EditorGimmick& g = g_Items[g_DragIndex];
				float mt = v.yToTime(mouse.y);
				if (g_Drag == Drag::Move)
				{
					float len = g.end - g.start;
					float ns = std::max(0.0f, snap(mt - g_DragGrab));
					if (!Overlaps(ns, ns + len, g_DragIndex)) { g.start = ns; g.end = ns + len; }
					ImGui::SetTooltip(u8"始まり %s", BarBeat(t, g.start).c_str());
				}
				else
				{
					float ne = snap(mt);
					if (ne > g.start + minLen - 0.0001f && !Overlaps(g.start, ne, g_DragIndex)) g.end = ne;
					ImGui::SetTooltip(u8"終わり %s（%.2f 拍）", BarBeat(t, g.end).c_str(), (g.end - g.start) / spb);
				}
			}
			else
			{
				g_DragIndex = -1;
				g_Drag = Drag::None;
				SortKeepSelection();
			}
			return true;
		}

		if (!inCol) return false;	// 列の外は、いつもの譜面の操作

		// ---- 列の上 ----
		if (hoverPart == Drag::Move) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		else if (hoverPart == Drag::Tail) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		// 置く種類を選んでいる時：何もない所に、置いた時の形をうっすら出す
		if (g_PlaceKind >= 0 && hoverIndex < 0)
		{
			float s = std::max(0.0f, snap(v.yToTime(mouse.y)));
			float e = s + spb * (float)std::max(1, t.beatsPerMeasure);
			ImU32 c = kKinds[g_PlaceKind].color;
			float y0 = v.timeToY(s), y1 = v.timeToY(e);
			draw->AddRectFilled(ImVec2(cx - tailHalfW, y1), ImVec2(cx + tailHalfW, y0), WithAlpha(c, 50));
			draw->AddRectFilled(ImVec2(cx - headHalfW, y0 - kHeadH * 0.5f), ImVec2(cx + headHalfW, y0 + kHeadH * 0.5f), WithAlpha(c, 140), 4.0f);
			draw->AddLine(ImVec2(v.laneX0, y0), ImVec2(v.laneX1, y0), WithAlpha(c, 120), 1.0f);
			ImGui::SetTooltip(u8"%s：%s から 1 小節", kKinds[g_PlaceKind].label, BarBeat(t, s).c_str());
		}
		else if (hoverIndex >= 0 && hoverPart == Drag::None)
		{
			const auto& g = g_Items[hoverIndex];
			ImGui::SetTooltip(u8"%s（×%.2f）\n%s 〜 %s\n頭をドラッグ：動かす　尾をドラッグ：長さ　右クリック：消す",
				kKinds[KindOf(g)].label, g.speed, BarBeat(t, g.start).c_str(), BarBeat(t, g.end).c_str());
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (hoverIndex >= 0)
			{
				g_Selected = hoverIndex;
				if (hoverPart != Drag::None)
				{
					g_DragIndex = hoverIndex;
					g_Drag = hoverPart;
					g_DragGrab = v.yToTime(mouse.y) - g_Items[hoverIndex].start;
				}
			}
			else if (g_PlaceKind >= 0)
			{
				// 置く：1 小節ぶん。後ろの演出とぶつかるなら、そこまで縮める
				float s = std::max(0.0f, snap(v.yToTime(mouse.y)));
				float e = s + spb * (float)std::max(1, t.beatsPerMeasure);
				for (const auto& o : g_Items)
					if (o.start >= s - 0.0001f && o.start < e) e = o.start;
				if (e - s < minLen - 0.0001f || Overlaps(s, e, -1))
					g_Message = u8"ほかの速さの演出と重なるので置けません";
				else
				{
					EditorGimmick g;
					g.start = s;
					g.end = e;
					g.speed = kKinds[g_PlaceKind].speed;
					float lenBeats = (e - s) / spb;
					g.easeIn = g.easeOut = std::min(0.5f, lenBeats * 0.25f);	// 短い演出は入り・抜けも短く
					g_Items.push_back(g);
					g_Selected = (int)g_Items.size() - 1;
					SortKeepSelection();
					g_Message = std::string(kKinds[g_PlaceKind].label) + u8" を置きました";
				}
			}
			else g_Selected = -1;	// 何もない所：選ぶのをやめる
		}
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			if (hoverIndex >= 0)
			{
				g_Items.erase(g_Items.begin() + hoverIndex);
				g_Selected = -1;
				g_Message = u8"消しました";
			}
			else g_PlaceKind = -1;	// 何もない所の右クリック：置くのをやめる
		}
		return true;	// 列の上は、ノーツの操作をしない
	}
}
