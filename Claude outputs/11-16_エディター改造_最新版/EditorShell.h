#pragma once
// ============================================================
// EditorShell : エディターの「外側」（Unity / クリップスタジオ風）
//
//   ┌ メニューバー（ファイル / 編集 / 表示 / 再生）───────────┐
//   ├ ツールバー（アイコンのボタン）                         ┤
//   ├──────┬───────────────────┬───────┤
//   │ ノーツ │                              │ 一覧   │
//   │ 設定   │   まんなか（2D 譜面 / 3D）     │ 判定円 │
//   ├──────┴───────────────────┴───────┤
//   └ 下のバー（再生・タイムライン）                          ┘
//   窓はドラッグで好きな場所へくっつけられる（ドッキング）。並べ方は imgui.ini に残る
//
// 新しい物を足すとき
//   ・操作（ボタン・メニュー・ショートカット）… AddCommand を 1 回呼ぶだけ。メニューにもツールバーにも出る
//   ・窓 … if (EditorShell::BeginPanel(u8"名前")) { 中身 } EditorShell::EndPanel();
//          （初めて出したときの場所は DefaultDock で決める）
//   ・アイコン … asset/icons/<名前>.png を置くだけ（無ければ文字で出る）
// ============================================================
#include <functional>
#include <string>
#include "imgui.h"

struct ID3D11Device;

// 操作 1 つ分（メニュー・ツールバー・ショートカットは全部これから作られる）
struct EditorCommand
{
	std::string id;					// 中で使う名前（"save" など。重ならないように）
	std::string label;				// 画面に出る名前（u8"保存"）
	std::string icon;				// アイコンの名前（asset/icons/<icon>.png）。空ならアイコンなし
	std::string menu;				// どのメニューに入れるか（u8"ファイル" など）。空ならメニューに出さない
	ImGuiKeyChord key = 0;			// ショートカット（ImGuiMod_Ctrl | ImGuiKey_S など）。0 ならなし
	bool toolbar = false;			// ツールバーに出すか
	bool separatorBefore = false;	// ツールバー・メニューで、この前に区切りを入れるか
	std::function<void()> run;			// 押したときにすること
	std::function<bool()> isOn;			// 「今これを選んでいる」表示（ツールの切り替え用。なくてよい）
	std::function<bool()> isEnabled;	// 押せるか（なくてよい＝いつでも押せる）
};

// 窓を最初にどこへ置くか
enum class EditorDock { Center, Left, LeftBottom, Right, Bottom, Floating };

namespace EditorShell
{
	// Game::Initialize で、ImGui_ImplDX11_Init の後に 1 回
	void Init(ID3D11Device* device);
	// Game::Render の UI の一番最初に、毎フレーム（メニュー・ツールバー・ドッキングの土台）
	void BeginFrame();

	// ---- 操作 ----
	void AddCommand(const EditorCommand& cmd);
	void RunCommand(const std::string& id);

	// ---- 窓 ----
	//   新しい窓：if (BeginPanel(u8"名前")) { … } EndPanel();   ※ EndPanel は必ず呼ぶ
	bool BeginPanel(const char* name, ImGuiWindowFlags flags = 0);
	void EndPanel();
	//   最初にどこへ置くか（Init の後、最初のフレームまでに呼ぶ）。今ある窓の名前でもよい
	void DefaultDock(const char* windowName, EditorDock where);
	//   今ある窓（ImGui::Begin を自分で呼んでいる窓）に付けるフラグ：動かせる・くっつけられる
	ImGuiWindowFlags PanelFlags();

	// まんなか（どの窓もくっついていない所）の画面の範囲。3D の表示やマウスの範囲に使う
	ImVec2 CentralPos();
	ImVec2 CentralSize();
	bool IsInCentral(ImVec2 screenPos);

	// ---- アイコン ----
	//   asset/icons/<name>.png を読む（1 回だけ。無ければ nullptr）
	ImTextureID Icon(const std::string& name);
	//   アイコンのボタン。アイコンが無ければ文字のボタンになる。選択中なら色を付ける
	bool IconButton(const char* id, const std::string& icon, const char* label, float size, bool selected = false,
		const char* tooltip = nullptr);
}
