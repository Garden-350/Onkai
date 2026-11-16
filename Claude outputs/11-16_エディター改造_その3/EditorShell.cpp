#include "pch.h"
#include "EditorShell.h"
#include "imgui_internal.h"		// DockBuilder（最初の並べ方を作る）
#include "WICTextureLoader.h"	// アイコンの PNG を読む（DirectXTK）
#include <map>
#include <vector>
#include <wrl/client.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include "json.hpp"			// 窓の背景の設定を editor_settings.json に残す

namespace
{
	ID3D11Device* s_Device = nullptr;

	// ---- 操作 ----
	std::vector<EditorCommand> s_Commands;
	std::vector<std::string> s_MenuOrder;		// メニューの並び（最初に出てきた順）

	// ---- 窓 ----
	struct PanelState { std::string name; bool open = true; };
	std::vector<PanelState> s_Panels;			// BeginPanel で作った窓（表示メニューに出る）
	std::vector<bool> s_PanelStack;				// BeginPanel で ImGui::Begin を呼んだか（EndPanel 用）
	std::vector<std::pair<std::string, EditorDock>> s_DefaultDocks;
	bool s_ResetLayout = false;

	// ---- まんなかの範囲 ----
	ImVec2 s_CentralPos(0, 0), s_CentralSize(0, 0);

	// ---- 絵（アイコン・ノーツ・背景）。1 回読んだら覚えておく ----
	struct TextureEntry
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		ImVec2 size = ImVec2(0, 0);		// 絵の大きさ（ピクセル）
	};
	std::map<std::string, TextureEntry> s_Icons;

	// ---- 窓の背景の絵 ----
	struct PanelBg
	{
		std::string path;		// 絵のファイル（空ならなし）
		float alpha = 0.35f;	// 濃さ（0〜1）
		int fit = 0;			// 0 いっぱいに（はみ出す所は切る） / 1 全体を見せる
	};
	std::map<std::string, PanelBg> s_PanelBg;		// 窓の名前 → 背景
	std::vector<std::string> s_BgPanels;			// PanelBackground を呼んだ窓（設定の一覧に出す）
	std::vector<std::filesystem::path> s_ImageFolders;	// 背景に選べる絵を探すフォルダ
	struct ImageItem { std::string path, label; };
	std::vector<ImageItem> s_Images;				// 見つかった絵
	bool s_ImagesScanned = false;
	bool s_ShowBgWindow = false;					// 「窓の背景」の設定の窓を出すか
	int s_BgTarget = 0;								// 設定の窓で今選んでいる窓
	const char* kSettingsFile = "editor_settings.json";

	// UTF-8 の文字列 → Windows のワイド文字列（日本語のパスも読めるように）
	std::wstring WidenUtf8(const std::string& s)
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(n > 0 ? n - 1 : 0, L'\0');
		if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
		return w;
	}

	// "設定###LeftPanel" → "設定"（### から後ろは中の名前なので画面には出さない）
	std::string ShownName(const std::string& name)
	{
		size_t p = name.find("###");
		return p == std::string::npos ? name : name.substr(0, p);
	}

	void SaveSettings()
	{
		nlohmann::json j = nlohmann::json::object();
		nlohmann::json bg = nlohmann::json::object();
		for (const auto& kv : s_PanelBg)
			if (!kv.second.path.empty())
				bg[kv.first] = { { "path", kv.second.path }, { "alpha", kv.second.alpha }, { "fit", kv.second.fit } };
		j["panelBackgrounds"] = bg;
		std::ofstream f(kSettingsFile);
		if (f.is_open()) f << j.dump(2);
	}

	void LoadSettings()
	{
		std::ifstream f(kSettingsFile);
		if (!f.is_open()) return;
		try
		{
			nlohmann::json j;
			f >> j;
			if (j.contains("panelBackgrounds") && j["panelBackgrounds"].is_object())
				for (auto it = j["panelBackgrounds"].begin(); it != j["panelBackgrounds"].end(); ++it)
				{
					PanelBg b;
					b.path = it.value().value("path", std::string());
					b.alpha = it.value().value("alpha", 0.35f);
					b.fit = it.value().value("fit", 0);
					s_PanelBg[it.key()] = b;
				}
		}
		catch (...) {}		// 壊れていたら気にしない（次に保存したときに作り直される）
	}

	// 背景に選べる絵を探す（png / jpg / bmp）
	void ScanImages()
	{
		namespace fs = std::filesystem;
		s_Images.clear();
		for (const auto& folder : s_ImageFolders)
		{
			std::error_code ec;
			if (!fs::exists(folder, ec)) continue;
			std::string tag = folder.is_absolute() ? std::string(u8"[ゲーム] ") : std::string();
			for (fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
				it != end && s_Images.size() < 3000; it.increment(ec))
			{
				if (ec) break;
				if (!it->is_regular_file(ec)) continue;
				std::string ext = it->path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
				if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp") continue;
				ImageItem item;
				item.path = it->path().generic_u8string();
				item.label = tag + fs::relative(it->path(), folder, ec).generic_u8string();
				if (ec) { item.label = tag + it->path().filename().u8string(); ec.clear(); }
				s_Images.push_back(item);
			}
		}
		std::sort(s_Images.begin(), s_Images.end(), [](const ImageItem& a, const ImageItem& b) { return a.label < b.label; });
		s_ImagesScanned = true;
	}

	// 「窓の背景」の設定の窓
	void DrawBackgroundWindow()
	{
		if (!s_ShowBgWindow) return;
		ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(u8"窓の背景", &s_ShowBgWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
		{
			if (!s_ImagesScanned) ScanImages();
			if (s_BgPanels.empty())
			{
				ImGui::TextDisabled(u8"背景を付けられる窓がまだありません");
				ImGui::End();
				return;
			}
			s_BgTarget = std::clamp(s_BgTarget, 0, (int)s_BgPanels.size() - 1);

			// どの窓の背景か
			ImGui::SetNextItemWidth(200);
			if (ImGui::BeginCombo(u8"窓", ShownName(s_BgPanels[s_BgTarget]).c_str()))
			{
				for (int i = 0; i < (int)s_BgPanels.size(); i++)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(ShownName(s_BgPanels[i]).c_str(), i == s_BgTarget)) s_BgTarget = i;
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			const std::string target = s_BgPanels[s_BgTarget];
			PanelBg& bg = s_PanelBg[target];
			bool changed = false;

			ImGui::SetNextItemWidth(200);
			ImGui::SliderFloat(u8"濃さ", &bg.alpha, 0.05f, 1.0f, "%.2f");
			if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
			changed |= ImGui::RadioButton(u8"いっぱいに", &bg.fit, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton(u8"全体を見せる", &bg.fit, 1);

			if (ImGui::Button(u8"なしにする")) { bg.path.clear(); changed = true; }
			ImGui::SameLine();
			if (ImGui::Button(u8"全部の窓に同じ絵"))
			{
				PanelBg copy = bg;
				for (const auto& name : s_BgPanels) s_PanelBg[name] = copy;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button(u8"探し直す")) ScanImages();

			static char filter[64] = "";
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##filter", u8"絞り込み（ファイル名の一部）", filter, sizeof(filter));
			ImGui::TextDisabled(u8"今：%s", bg.path.empty() ? u8"なし" : bg.path.c_str());

			// 絵の一覧（クリックでその窓の背景になる。マウスを乗せると小さく見える）
			if (ImGui::BeginChild("##images", ImVec2(0, 0), true))
			{
				int shown = 0;
				for (const auto& img : s_Images)
				{
					if (filter[0] && img.label.find(filter) == std::string::npos) continue;
					shown++;
					if (ImGui::Selectable(img.label.c_str(), img.path == bg.path))
					{
						bg.path = img.path;
						changed = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImTextureID tex = EditorShell::Texture(img.path);
						ImVec2 ts = EditorShell::TextureSize(img.path);
						if (tex && ts.x > 0 && ts.y > 0 && ImGui::BeginTooltip())
						{
							float k = std::min(200.0f / ts.x, 200.0f / ts.y);
							ImGui::Image(tex, ImVec2(ts.x * k, ts.y * k));
							ImGui::Text("%d x %d", (int)ts.x, (int)ts.y);
							ImGui::EndTooltip();
						}
					}
				}
				if (shown == 0)
					ImGui::TextDisabled(u8"絵が見つかりません（asset フォルダに png / jpg を入れて「探し直す」）");
			}
			ImGui::EndChild();
			if (changed) SaveSettings();
		}
		ImGui::End();
	}

	constexpr float kToolbarHeight = 44.0f;		// ツールバーの高さ
	constexpr float kToolbarIcon = 26.0f;		// ツールバーのアイコンの大きさ

	PanelState& FindPanel(const char* name)
	{
		for (auto& p : s_Panels) if (p.name == name) return p;
		s_Panels.push_back({ name, true });
		return s_Panels.back();
	}

	// "Ctrl+S" のような表示
	std::string ChordText(ImGuiKeyChord chord)
	{
		if (!chord) return "";
		std::string s;
		if (chord & ImGuiMod_Ctrl)  s += "Ctrl+";
		if (chord & ImGuiMod_Shift) s += "Shift+";
		if (chord & ImGuiMod_Alt)   s += "Alt+";
		s += ImGui::GetKeyName((ImGuiKey)(chord & ~ImGuiMod_Mask_));
		return s;
	}

	bool Enabled(const EditorCommand& c) { return !c.isEnabled || c.isEnabled(); }
	bool On(const EditorCommand& c) { return c.isOn && c.isOn(); }

	// ---- 見た目（クリップスタジオ風の暗い灰色 ＋ 青） ----
	void ApplyTheme()
	{
		ImGuiStyle& s = ImGui::GetStyle();
		ImGui::StyleColorsDark(&s);
		s.WindowRounding = 4.0f;
		s.ChildRounding = 4.0f;
		s.FrameRounding = 4.0f;
		s.PopupRounding = 4.0f;
		s.GrabRounding = 4.0f;
		s.TabRounding = 4.0f;
		s.ScrollbarRounding = 6.0f;
		s.WindowPadding = ImVec2(8, 8);
		s.FramePadding = ImVec2(6, 4);
		s.ItemSpacing = ImVec2(6, 5);
		s.WindowBorderSize = 1.0f;
		s.TabBorderSize = 0.0f;
		s.WindowMenuButtonPosition = ImGuiDir_None;		// 窓の左上の三角は出さない

		ImVec4* c = s.Colors;
		const ImVec4 bg(0.17f, 0.17f, 0.18f, 1.0f);
		const ImVec4 panel(0.20f, 0.20f, 0.21f, 1.0f);
		const ImVec4 frame(0.13f, 0.13f, 0.14f, 1.0f);
		const ImVec4 hover(0.30f, 0.30f, 0.33f, 1.0f);
		const ImVec4 accent(0.24f, 0.52f, 0.95f, 1.0f);
		c[ImGuiCol_WindowBg] = panel;
		c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);		// 子の窓は透明（窓の背景の絵が見えるように）
		c[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.16f, 0.98f);
		c[ImGuiCol_MenuBarBg] = bg;
		c[ImGuiCol_Border] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		c[ImGuiCol_FrameBg] = frame;
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
		c[ImGuiCol_TitleBg] = bg;
		c[ImGuiCol_TitleBgActive] = bg;
		c[ImGuiCol_TitleBgCollapsed] = bg;
		c[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		c[ImGuiCol_ButtonHovered] = hover;
		c[ImGuiCol_ButtonActive] = accent;
		c[ImGuiCol_Header] = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		c[ImGuiCol_HeaderHovered] = hover;
		c[ImGuiCol_HeaderActive] = accent;
		c[ImGuiCol_CheckMark] = accent;
		c[ImGuiCol_SliderGrab] = accent;
		c[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.65f, 1.0f, 1.0f);
		c[ImGuiCol_Tab] = bg;
		c[ImGuiCol_TabHovered] = hover;
		c[ImGuiCol_TabActive] = panel;
		c[ImGuiCol_TabUnfocused] = bg;
		c[ImGuiCol_TabUnfocusedActive] = panel;
		c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.6f);
		c[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_SeparatorHovered] = accent;
		c[ImGuiCol_SeparatorActive] = accent;
		c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
		c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.4f);
	}

	// ---- 日本語のフォント（メイリオ。無ければ ImGui の標準のフォント） ----
	void LoadFonts()
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();
		const char* kFontPath = "C:\\Windows\\Fonts\\meiryo.ttc";
		ImFont* font = nullptr;
		if (GetFileAttributesA(kFontPath) != INVALID_FILE_ATTRIBUTES)
			font = io.Fonts->AddFontFromFileTTF(kFontPath, 17.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
		if (!font) io.Fonts->AddFontDefault();
	}

	// ---- 最初の並べ方 ----
	void BuildDefaultLayout(ImGuiID dockId, ImVec2 size)
	{
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockId, size);

		ImGuiID center = dockId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.20f, nullptr, &center);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.08f, nullptr, &center);
		ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.62f, nullptr, &left);

		for (const auto& d : s_DefaultDocks)
		{
			ImGuiID node = 0;
			switch (d.second)
			{
			case EditorDock::Center:     node = center; break;
			case EditorDock::Left:       node = left; break;
			case EditorDock::LeftBottom: node = leftBottom; break;
			case EditorDock::Right:      node = right; break;
			case EditorDock::Bottom:     node = bottom; break;
			case EditorDock::Floating:   break;
			}
			if (node) ImGui::DockBuilderDockWindow(d.first.c_str(), node);
		}
		ImGui::DockBuilderFinish(dockId);
	}

	// ---- メニューバー ----
	void DrawMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) return;
		for (const auto& menu : s_MenuOrder)
		{
			if (!ImGui::BeginMenu(menu.c_str())) continue;
			for (auto& c : s_Commands)
			{
				if (c.menu != menu) continue;
				if (c.separatorBefore) ImGui::Separator();
				std::string sc = ChordText(c.key);
				if (ImGui::MenuItem(c.label.c_str(), sc.empty() ? nullptr : sc.c_str(), On(c), Enabled(c)) && c.run)
					c.run();
			}
			ImGui::EndMenu();
		}
		// 表示：窓の表示・非表示と、並べ方を元に戻す
		if (ImGui::BeginMenu(u8"表示"))
		{
			for (auto& p : s_Panels)
				ImGui::MenuItem(p.name.c_str(), nullptr, &p.open);
			ImGui::Separator();
			if (ImGui::MenuItem(u8"窓の背景...", nullptr, s_ShowBgWindow)) s_ShowBgWindow = !s_ShowBgWindow;
			if (ImGui::MenuItem(u8"窓の並べ方を元に戻す")) s_ResetLayout = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// ---- ツールバー ----
	void DrawToolbar(const ImGuiViewport* vp)
	{
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kToolbarHeight));
		ImGui::SetNextWindowViewport(vp->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 7));
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::Begin("##Toolbar", nullptr, flags);
		ImGui::PopStyleVar(2);

		bool first = true;
		for (auto& c : s_Commands)
		{
			if (!c.toolbar) continue;
			if (!first)
			{
				ImGui::SameLine();
				if (c.separatorBefore)
				{
					ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
					ImGui::SameLine();
				}
			}
			first = false;
			std::string tip = c.label;
			std::string sc = ChordText(c.key);
			if (!sc.empty()) tip += "  (" + sc + ")";
			ImGui::BeginDisabled(!Enabled(c));
			if (EditorShell::IconButton(c.id.c_str(), c.icon, c.label.c_str(), kToolbarIcon, On(c), tip.c_str()) && c.run)
				c.run();
			ImGui::EndDisabled();
		}
		ImGui::End();
	}

	// ---- ショートカット ----
	void HandleShortcuts()
	{
		if (ImGui::GetIO().WantTextInput) return;		// 文字を打っている間は効かせない
		for (auto& c : s_Commands)
			if (c.key && c.run && Enabled(c) && ImGui::IsKeyChordPressed(c.key))
				c.run();
	}
}

namespace EditorShell
{
	void Init(ID3D11Device* device)
	{
		s_Device = device;
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// 窓をくっつけられるようにする
		io.ConfigWindowsMoveFromTitleBarOnly = true;			// 窓はタイトルだけで動かす（譜面の上のドラッグで窓が動かないように）
		LoadFonts();
		LoadSettings();		// 窓の背景の設定
		ApplyTheme();
	}

	void BeginFrame()
	{
		HandleShortcuts();
		DrawMenuBar();

		const ImGuiViewport* vp = ImGui::GetMainViewport();
		DrawToolbar(vp);

		// ツールバーの下を全部ドッキングの場所にする（まんなかは透明：何もくっついていなければ 3D が見える）
		ImVec2 pos(vp->WorkPos.x, vp->WorkPos.y + kToolbarHeight);
		ImVec2 size(vp->WorkSize.x, vp->WorkSize.y - kToolbarHeight);
		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		ImGui::SetNextWindowViewport(vp->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
		ImGui::Begin("##DockHost", nullptr, hostFlags);
		ImGui::PopStyleVar(3);

		ImGuiID dockId = ImGui::GetID("MainDockSpace");
		if (s_ResetLayout || ImGui::DockBuilderGetNode(dockId) == nullptr)
		{
			BuildDefaultLayout(dockId, size);
			s_ResetLayout = false;
		}
		ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

		if (ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockId))
		{
			s_CentralPos = central->Pos;
			s_CentralSize = central->Size;
		}
		ImGui::End();

		DrawBackgroundWindow();		// 「表示 → 窓の背景...」の設定の窓
	}

	void AddCommand(const EditorCommand& cmd)
	{
		for (auto& c : s_Commands)
			if (c.id == cmd.id) { c = cmd; return; }		// 同じ id なら上書き
		s_Commands.push_back(cmd);
		if (!cmd.menu.empty() &&
			std::find(s_MenuOrder.begin(), s_MenuOrder.end(), cmd.menu) == s_MenuOrder.end())
			s_MenuOrder.push_back(cmd.menu);
	}

	void RunCommand(const std::string& id)
	{
		for (auto& c : s_Commands)
			if (c.id == id && c.run && Enabled(c)) { c.run(); return; }
	}

	bool BeginPanel(const char* name, ImGuiWindowFlags flags)
	{
		PanelState& p = FindPanel(name);
		if (!p.open) { s_PanelStack.push_back(false); return false; }
		bool visible = ImGui::Begin(name, &p.open, flags);
		PanelBackground();
		s_PanelStack.push_back(true);
		return visible;
	}

	void EndPanel()
	{
		if (s_PanelStack.empty()) return;
		bool began = s_PanelStack.back();
		s_PanelStack.pop_back();
		if (began) ImGui::End();
	}

	void DefaultDock(const char* windowName, EditorDock where)
	{
		s_DefaultDocks.push_back({ windowName, where });
	}

	ImGuiWindowFlags PanelFlags()
	{
		return ImGuiWindowFlags_NoCollapse;
	}

	ImVec2 CentralPos() { return s_CentralPos; }
	ImVec2 CentralSize() { return s_CentralSize; }
	bool IsInCentral(ImVec2 p)
	{
		return p.x >= s_CentralPos.x && p.y >= s_CentralPos.y &&
			p.x <= s_CentralPos.x + s_CentralSize.x && p.y <= s_CentralPos.y + s_CentralSize.y;
	}

	ImTextureID Texture(const std::string& path)
	{
		if (path.empty() || !s_Device) return nullptr;
		auto it = s_Icons.find(path);
		if (it == s_Icons.end())
		{
			TextureEntry e;
			std::wstring wpath = WidenUtf8(path);		// 日本語のパスも読める
			if (FAILED(DirectX::CreateWICTextureFromFile(s_Device, wpath.c_str(), nullptr, e.srv.GetAddressOf())))
				e.srv.Reset();					// 無いときも覚えておく（毎フレーム探さない）
			else
			{
				// 絵の大きさを覚える（背景の合わせ方に使う）
				Microsoft::WRL::ComPtr<ID3D11Resource> res;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
				e.srv->GetResource(res.GetAddressOf());
				if (res && SUCCEEDED(res.As(&tex2d)))
				{
					D3D11_TEXTURE2D_DESC d = {};
					tex2d->GetDesc(&d);
					e.size = ImVec2((float)d.Width, (float)d.Height);
				}
			}
			it = s_Icons.emplace(path, e).first;
		}
		return (ImTextureID)it->second.srv.Get();
	}

	ImVec2 TextureSize(const std::string& path)
	{
		if (!Texture(path)) return ImVec2(0, 0);
		return s_Icons[path].size;
	}

	void AddImageFolder(const std::filesystem::path& folder)
	{
		s_ImageFolders.push_back(folder);
		s_ImagesScanned = false;
	}

	// ============================================================
	// 今の窓の後ろに、選んだ背景の絵を描く（ImGui::Begin の直後に呼ぶ）
	// ============================================================
	void PanelBackground()
	{
		ImGuiWindow* w = ImGui::GetCurrentWindow();
		if (!w) return;
		std::string name = w->Name;
		if (std::find(s_BgPanels.begin(), s_BgPanels.end(), name) == s_BgPanels.end())
			s_BgPanels.push_back(name);		// 設定の一覧に出す

		auto it = s_PanelBg.find(name);
		if (it == s_PanelBg.end() || it->second.path.empty()) return;
		const PanelBg& bg = it->second;
		ImTextureID tex = Texture(bg.path);
		ImVec2 ts = TextureSize(bg.path);
		if (!tex || ts.x < 1.0f || ts.y < 1.0f) return;

		// 中身の範囲（タイトル・タブ・スクロールバーを除く）。スクロールしても絵は動かない
		ImVec2 a = w->InnerRect.Min, b = w->InnerRect.Max;
		float rw = b.x - a.x, rh = b.y - a.y;
		if (rw < 1.0f || rh < 1.0f) return;
		float boxAspect = rw / rh, imgAspect = ts.x / ts.y;
		ImVec2 uv0(0, 0), uv1(1, 1);
		if (bg.fit == 0)
		{
			// いっぱいに：窓を全部うめる。はみ出す所は真ん中を残して切る
			if (imgAspect > boxAspect) { float u = boxAspect / imgAspect; uv0.x = (1.0f - u) * 0.5f; uv1.x = uv0.x + u; }
			else { float v = imgAspect / boxAspect; uv0.y = (1.0f - v) * 0.5f; uv1.y = uv0.y + v; }
		}
		else
		{
			// 全体を見せる：絵を全部入れる。余りは何も描かない
			float dw = rw, dh = rw / imgAspect;
			if (dh > rh) { dh = rh; dw = rh * imgAspect; }
			a = ImVec2(a.x + (rw - dw) * 0.5f, a.y + (rh - dh) * 0.5f);
			b = ImVec2(a.x + dw, a.y + dh);
		}
		int alpha = (int)(std::clamp(bg.alpha, 0.0f, 1.0f) * 255.0f);
		w->DrawList->AddImage(tex, a, b, uv0, uv1, IM_COL32(255, 255, 255, alpha));
	}

	ImTextureID Icon(const std::string& name)
	{
		if (name.empty()) return nullptr;
		return Texture("asset/icons/" + name + ".png");
	}

	bool IconButton(const char* id, const std::string& icon, const char* label, float size, bool selected,
		const char* tooltip)
	{
		ImGui::PushID(id);
		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
		bool pressed = false;
		if (ImTextureID tex = Icon(icon))
			pressed = ImGui::ImageButton("##b", tex, ImVec2(size, size));
		else
			pressed = ImGui::Button(label, ImVec2(0, size + ImGui::GetStyle().FramePadding.y * 2.0f - 2.0f));
		if (selected) ImGui::PopStyleColor(2);
		if (tooltip && *tooltip) ImGui::SetItemTooltip("%s", tooltip);
		ImGui::PopID();
		return pressed;
	}
}
