#include "pch.h"
#include "EditorShell.h"
#include "imgui_internal.h"		// DockBuilder（最初の並べ方を作る）
#include "WICTextureLoader.h"	// アイコンの PNG を読む（DirectXTK）
#include <map>
#include <vector>
#include <wrl/client.h>

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

	// ---- アイコン ----
	std::map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> s_Icons;

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
		c[ImGuiCol_ChildBg] = panel;
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
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			std::wstring wpath(path.begin(), path.end());		// 英数字のパスだけを使う
			if (FAILED(DirectX::CreateWICTextureFromFile(s_Device, wpath.c_str(), nullptr, srv.GetAddressOf())))
				srv.Reset();					// 無いときも覚えておく（毎フレーム探さない）
			it = s_Icons.emplace(path, srv).first;
		}
		return (ImTextureID)it->second.Get();
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
