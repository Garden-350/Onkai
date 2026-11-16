#include "pch.h"
#include "FileBrowser.h"
#include "EditorShell.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	const char* kChartFolder = "charts";		// 譜面のフォルダ（SaveChart / LoadChart と同じ）
	const char* kAudioFolder = "asset/bgm";		// 曲のフォルダ（LoadChart の "asset/bgm/" と同じ）

	struct Entry
	{
		std::string name;		// 中で使う名前（LoadChart などにそのまま渡す）
		std::string label;		// 画面に出す名前（UTF-8）
		fs::file_time_type time;	// 最後に保存した時刻（新しい順に並べる用）
	};
	std::vector<Entry> s_Charts, s_Audios;
	double s_LastScan = -100.0;
	bool s_NewestFirst = false;		// 新しい順に並べるか（いつもは名前順）
	char s_Filter[64] = "";
	std::string s_Pending;			// 開くか聞いている譜面

	std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	// フォルダの中の、指定の拡張子のファイルを集める
	std::vector<Entry> Scan(const char* folder, std::initializer_list<const char*> exts, bool stripExt)
	{
		std::vector<Entry> out;
		std::error_code ec;
		for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
		{
			if (!it->is_regular_file(ec)) continue;
			std::string ext = Lower(it->path().extension().string());
			bool ok = false;
			for (const char* e : exts) if (ext == e) ok = true;
			if (!ok) continue;
			Entry e;
			fs::path p = stripExt ? it->path().stem() : it->path().filename();
			e.name = p.string();		// 保存・読み込みと同じ文字コード
			e.label = p.u8string();		// 画面は UTF-8
			e.time = it->last_write_time(ec);
			out.push_back(e);
		}
		return out;
	}

	void Sort(std::vector<Entry>& v)
	{
		if (s_NewestFirst)
			std::sort(v.begin(), v.end(), [](const Entry& a, const Entry& b) { return a.time > b.time; });
		else
			std::sort(v.begin(), v.end(), [](const Entry& a, const Entry& b) { return Lower(a.label) < Lower(b.label); });
	}

	void Rescan()
	{
		s_Charts = Scan(kChartFolder, { ".json" }, true);
		s_Audios = Scan(kAudioFolder, { ".wav", ".mp3" }, false);
		Sort(s_Charts);
		Sort(s_Audios);
		s_LastScan = ImGui::GetTime();
	}

	bool PassFilter(const Entry& e)
	{
		return !s_Filter[0] || Lower(e.label).find(Lower(s_Filter)) != std::string::npos;
	}
}

namespace FileBrowser
{
	void DrawPanel(const std::string& currentChart, const std::string& currentAudio, const Actions& actions)
	{
		if (EditorShell::BeginPanel(u8"ファイル"))
		{
			if (ImGui::GetTime() - s_LastScan > 2.0) Rescan();		// 2 秒ごとにフォルダを見直す

			ImGui::SetNextItemWidth(-90);
			ImGui::InputTextWithHint("##filter", u8"絞り込み", s_Filter, sizeof(s_Filter));
			ImGui::SameLine();
			if (ImGui::Checkbox(u8"新しい順", &s_NewestFirst)) Rescan();

			if (ImGui::BeginTabBar("##filetabs"))
			{
				// ---- 譜面 ----
				if (ImGui::BeginTabItem(u8"譜面"))
				{
					ImGui::TextDisabled(u8"今：%s（クリックで開く）", currentChart.c_str());
					if (ImGui::BeginChild("##charts", ImVec2(0, 0), false))
					{
						for (const auto& e : s_Charts)
						{
							if (!PassFilter(e)) continue;
							ImGui::PushID(e.name.c_str());
							if (ImGui::Selectable(e.label.c_str(), e.name == currentChart))
							{
								s_Pending = e.name;
								ImGui::OpenPopup(u8"譜面を開く");
							}
							// 開く前に聞く（保存していない変更が消えないように）
							if (ImGui::BeginPopupModal(u8"譜面を開く", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
							{
								ImGui::Text(u8"「%s」を開きますか？", e.label.c_str());
								ImGui::TextDisabled(u8"今の譜面（%s）の保存していない変更は消えます", currentChart.c_str());
								if (ImGui::Button(u8"保存してから開く"))
								{
									if (!actions.saveChart || actions.saveChart())
										if (actions.openChart) actions.openChart(s_Pending);
									ImGui::CloseCurrentPopup();
								}
								ImGui::SameLine();
								if (ImGui::Button(u8"保存しないで開く"))
								{
									if (actions.openChart) actions.openChart(s_Pending);
									ImGui::CloseCurrentPopup();
								}
								ImGui::SameLine();
								if (ImGui::Button(u8"やめる")) ImGui::CloseCurrentPopup();
								ImGui::EndPopup();
							}
							ImGui::PopID();
						}
						if (s_Charts.empty()) ImGui::TextDisabled(u8"charts フォルダに譜面がありません");
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}

				// ---- 曲 ----
				if (ImGui::BeginTabItem(u8"曲"))
				{
					ImGui::TextDisabled(u8"今：%s（クリックでこの曲にする）", currentAudio.empty() ? u8"なし" : currentAudio.c_str());
					if (ImGui::BeginChild("##audios", ImVec2(0, 0), false))
					{
						for (const auto& e : s_Audios)
						{
							if (!PassFilter(e)) continue;
							ImGui::PushID(e.name.c_str());
							if (ImGui::Selectable(e.label.c_str(), e.name == currentAudio) && actions.setAudio)
								actions.setAudio(e.name);
							ImGui::PopID();
						}
						if (s_Audios.empty()) ImGui::TextDisabled(u8"asset/bgm に曲（wav / mp3）がありません");
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}
		EditorShell::EndPanel();
	}
}
