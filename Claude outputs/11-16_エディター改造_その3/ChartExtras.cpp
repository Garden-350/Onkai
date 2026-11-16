#include "pch.h"
#include "ChartExtras.h"
#include "imgui.h"
#include "imgui_internal.h"	// BringWindowToDisplayFront（ゲームの所をふさぐ窓を一番上に）
#include <algorithm>
#include <cmath>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;

namespace
{
	// ---- 保存で消えないようにするための記憶 ----
	json g_Extra = json::object();		// トップの知らない項目（tracks, rolls など）
	json g_ExtraMeta = json::object();	// "meta" の中の知らない項目
	std::vector<EditorCameraKey> g_Camera;	// カメラの区切り

	// エディターが自分で読み書きしている項目（これ以外は「知らない項目」として、そのまま残す）
	const std::set<std::string> kKnownTop = { "meta", "notes", "beams", "homingBeams", "judgeCircles", "camera" };
	const std::set<std::string> kKnownMeta = { "bpm", "offset", "totalBeat", "audioFile", "totalNotes" };

	// ---- カメラの名前（ゲームの CameraTrack.h の CameraPresets と同じ順番・同じ値にする） ----
	struct PresetInfo { const char* name; float eye[3]; float target[3]; };
	const PresetInfo kPresets[] =
	{
		{ "default", {    0.0f,  630.0f,  -880.0f }, {   0.0f, -480.0f, 290.0f } },	// いつものカメラ（Stage1Scene が決める）
		{ "front",   {    0.0f,  150.0f, -2000.0f }, {   0.0f,  150.0f,   0.0f } },
		{ "low",     {    0.0f,  250.0f, -1250.0f }, {   0.0f, -430.0f, 450.0f } },
		{ "far",     {    0.0f,  850.0f, -1350.0f }, {   0.0f, -480.0f, 350.0f } },
		{ "high",    {    0.0f, 1300.0f,  -800.0f }, {   0.0f, -480.0f, 450.0f } },
		{ "right",   {  520.0f,  560.0f,  -900.0f }, {  60.0f, -480.0f, 350.0f } },
		{ "left",    { -520.0f,  560.0f,  -900.0f }, { -60.0f, -480.0f, 350.0f } },
		{ "rightup", {  420.0f, 1300.0f,  -900.0f }, {  40.0f, -480.0f, 400.0f } },
		{ "leftup",  { -420.0f, 1300.0f,  -900.0f }, { -40.0f, -480.0f, 400.0f } },
	};
	constexpr int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
	constexpr int kCustomIndex = kPresetCount;		// 一覧の最後が "custom"

	int PresetIndex(const std::string& name)
	{
		for (int i = 0; i < kPresetCount; i++) if (name == kPresets[i].name) return i;
		return (name == "custom") ? kCustomIndex : 0;
	}

	// ---- 時間の計算（ゲームの ChartObject::GetCameraPose と同じ数え方） ----
	float BeatSec(const EditorTimeInfo& t) { return (t.bpm > 0.0f) ? 60.0f / t.bpm : 0.5f; }
	float BarSec(const EditorTimeInfo& t) { return BeatSec(t) * (float)std::max(1, t.beatsPerMeasure); }
	float MeasureStart(const EditorTimeInfo& t, int measure) { return t.offset + (measure - 1) * BarSec(t); }	// 1 小節目 = offset
	int MeasureAt(const EditorTimeInfo& t, float sec) { return (int)std::floor((sec - t.offset) / BarSec(t)) + 1; }

	// ---- テストプレイ ----
	PROCESS_INFORMATION g_Proc = {};		// 起動中のゲーム（次のテストプレイの前に閉じる）
	bool g_TestAuto = false;				// オートで流すか
	bool g_TestRelease = false;				// Release の exe を使うか（いつもは Debug）
	int  g_TestBarsBefore = 1;				// 何小節前から始めるか
	std::string g_TestMessage;
	bool g_TestIsError = false;

	// ---- テストプレイの窓をエディターの上に重ねる ----
	//   ゲームの窓は「子どもの窓」にはしない（SetParent はしない）。
	//   枠を消した普通の窓のまま、エディターのまんなかの場所にぴったり重ねて、毎フレーム場所を合わせる。
	//   ・子どもの窓にすると、エディターの描画（FLIP のスワップチェイン）の下に隠れて見えない
	//   ・別のプロセスの子どもの窓は、ゲームの読み込み中にエディターまで止まる・キー入力が届きにくい
	bool g_EmbedGame = true;				// エディターに重ねるか（オフなら別の窓のまま）
	HWND g_GameWnd = nullptr;				// 重ねているゲームの窓
	LONG_PTR g_GameStyle = 0;				// 重ねる前の窓の形（戻すとき用）
	RECT g_GameRect = {};					// 重ねる前の窓の場所（戻すとき用）
	RECT g_LastRect = {};					// 最後に置いた場所（変わった時だけ動かす）
	int g_FindCooldown = 0;					// 窓を探す間隔（毎フレームは探さない）
	bool g_GameHidden = false;				// エディターを最小化したので隠しているか

	struct FindCtx { DWORD pid; HWND found; };
	BOOL CALLBACK FindGameWindow(HWND w, LPARAM lp)
	{
		FindCtx* f = reinterpret_cast<FindCtx*>(lp);
		DWORD pid = 0;
		GetWindowThreadProcessId(w, &pid);
		if (pid == f->pid && IsWindowVisible(w) && GetWindow(w, GW_OWNER) == nullptr)
		{
			f->found = w;
			return FALSE;		// 見つかった
		}
		return TRUE;
	}

	// ゲームがメッセージを受け取れるか（ゲームは窓を出してから読み込むので、読み込み中は触らずに待つ）
	bool IsGameReady(HWND w)
	{
		DWORD_PTR result = 0;
		return SendMessageTimeoutW(w, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 20, &result) != 0;
	}

	// 重ねるのをやめて、元の窓に戻す
	void ReleaseGameWindow()
	{
		if (g_GameWnd && IsWindow(g_GameWnd))
		{
			SetWindowLongPtrW(g_GameWnd, GWL_STYLE, g_GameStyle);
			SetWindowPos(g_GameWnd, nullptr, g_GameRect.left, g_GameRect.top,
				g_GameRect.right - g_GameRect.left, g_GameRect.bottom - g_GameRect.top,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);
		}
		g_GameWnd = nullptr;
		g_LastRect = {};
		g_GameHidden = false;
	}

	// まんなかを黒い窓でふさぐ
	//   ・ゲームは 16:9 なので、上下（左右）のすき間から後ろの譜面が見えないように
	//   ・すき間をクリックして、後ろの譜面にノーツを置いてしまわないように
	void DrawGameCover(float x, float y, float w, float h, bool ready)
	{
		if (w < 1.0f || h < 1.0f) return;
		ImGui::SetNextWindowPos(ImVec2(x, y));
		ImGui::SetNextWindowSize(ImVec2(w, h));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::Begin("##GameCover", nullptr, flags);
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());		// 譜面の窓より上に
		ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));
		if (ready)
			ImGui::TextDisabled(u8"テストプレイ中　Esc / Close game で終わる　（設定の「Show inside editor」をオフにすると別の窓）");
		else
			ImGui::TextDisabled(u8"ゲームを読み込み中…");
		ImGui::End();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	std::wstring Widen(const std::string& s)
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(n > 0 ? n - 1 : 0, L'\0');
		if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
		return w;
	}

	void CloseRunningGame()
	{
		if (!g_Proc.hProcess) return;
		DWORD code = 0;
		if (GetExitCodeProcess(g_Proc.hProcess, &code) && code == STILL_ACTIVE)
			TerminateProcess(g_Proc.hProcess, 0);		// 前のテストプレイがまだ開いていたら閉じる
		CloseHandle(g_Proc.hProcess);
		CloseHandle(g_Proc.hThread);
		g_Proc = {};
		g_GameWnd = nullptr;		// 窓はゲームと一緒に消える
		g_LastRect = {};
		g_GameHidden = false;
		g_FindCooldown = 0;
	}

	// 保存 → ゲームを startSec 秒から起動
	void LaunchTestPlay(const fs::path& gameRoot, const std::string& chartName, float startSec,
		const std::function<bool()>& saveToGame)
	{
		if (!saveToGame())
		{
			g_TestMessage = "Test play: save failed";
			g_TestIsError = true;
			return;
		}
		fs::path exe = gameRoot / "x64" / (g_TestRelease ? "Release" : "Debug") / "Sounder.exe";
		std::error_code ec;
		if (!fs::exists(exe, ec))
		{
			g_TestMessage = g_TestRelease ? "Test play: Release/Sounder.exe not found. Build the game (Release) first."
				: "Test play: Debug/Sounder.exe not found. Build the game (Debug) first.";
			g_TestIsError = true;
			return;
		}
		CloseRunningGame();

		// "…\Sounder.exe" --chart "名前" --start 32.5 [--auto]
		wchar_t startBuf[32];
		swprintf_s(startBuf, L"%.3f", std::max(0.0f, startSec));
		std::wstring cmd = L"\"" + exe.wstring() + L"\" --chart \"" + Widen(chartName) + L"\" --start " + startBuf;
		if (g_TestAuto) cmd += L" --auto";

		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		// 作業フォルダはゲームのフォルダ（VS の F5 と同じ。assets や charts をそこから読む）
		std::wstring workDir = gameRoot.wstring();
		if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
			nullptr, workDir.c_str(), &si, &g_Proc))
		{
			g_TestMessage = "Test play: could not start Sounder.exe (error " + std::to_string(GetLastError()) + ")";
			g_TestIsError = true;
			g_Proc = {};
			return;
		}
		char buf[96];
		sprintf_s(buf, "Test play: started from %.2f s", std::max(0.0f, startSec));
		g_TestMessage = buf;
		g_TestIsError = false;
	}

	// bar 小節目の頭から、g_TestBarsBefore 小節前の秒
	float StartSecForBar(const EditorTimeInfo& t, int bar)
	{
		int from = std::max(1, bar - g_TestBarsBefore);
		return std::max(0.0f, MeasureStart(t, from));
	}

	json Vec3Json(const float v[3]) { return json{ { "x", v[0] }, { "y", v[1] }, { "z", v[2] } }; }
	void ReadVec3(const json& j, float v[3])
	{
		v[0] = j.value("x", v[0]); v[1] = j.value("y", v[1]); v[2] = j.value("z", v[2]);
	}
}

namespace ChartExtras
{
	std::vector<EditorCameraKey>& CameraKeys() { return g_Camera; }

	// ============================================================
	// 読み込み：知らない項目とカメラを覚える
	// ============================================================
	void OnLoad(const json& j)
	{
		g_Extra = json::object();
		g_ExtraMeta = json::object();
		g_Camera.clear();
		if (!j.is_object()) return;

		for (auto it = j.begin(); it != j.end(); ++it)
			if (!kKnownTop.count(it.key())) g_Extra[it.key()] = it.value();

		if (j.contains("meta") && j["meta"].is_object())
			for (auto it = j["meta"].begin(); it != j["meta"].end(); ++it)
				if (!kKnownMeta.count(it.key())) g_ExtraMeta[it.key()] = it.value();

		if (j.contains("camera") && j["camera"].is_array())
		{
			for (const auto& c : j["camera"])
			{
				EditorCameraKey k;
				k.fromMeasure = c.value("fromMeasure", 1);
				k.preset = c.value("preset", std::string("default"));
				k.blendBeats = c.value("blendBeats", 2.0f);
				if (c.contains("eye") && c.contains("target"))
				{
					ReadVec3(c["eye"], k.eye);
					ReadVec3(c["target"], k.target);
					k.preset = "custom";
				}
				g_Camera.push_back(k);
			}
			std::sort(g_Camera.begin(), g_Camera.end(),
				[](const EditorCameraKey& a, const EditorCameraKey& b) { return a.fromMeasure < b.fromMeasure; });
		}
	}

	// ============================================================
	// 保存："meta" の知らない項目
	// ============================================================
	std::string MetaExtras()
	{
		std::string s;
		for (auto it = g_ExtraMeta.begin(); it != g_ExtraMeta.end(); ++it)
			s += ",\"" + it.key() + "\":" + it.value().dump();
		return s;
	}

	// ============================================================
	// 保存："camera" と、トップの知らない項目（tracks など）
	// ============================================================
	void WriteExtras(std::ostream& file)
	{
		if (!g_Camera.empty())
		{
			file << ",\n\"camera\":[\n";
			for (size_t i = 0; i < g_Camera.size(); i++)
			{
				const auto& k = g_Camera[i];
				json c = { { "fromMeasure", k.fromMeasure }, { "preset", k.preset }, { "blendBeats", k.blendBeats } };
				if (k.preset == "custom")
				{
					c["eye"] = Vec3Json(k.eye);
					c["target"] = Vec3Json(k.target);
				}
				file << c.dump() << (i + 1 < g_Camera.size() ? ",\n" : "\n");
			}
			file << "]";
		}
		for (auto it = g_Extra.begin(); it != g_Extra.end(); ++it)
			file << ",\n\"" << it.key() << "\":" << it.value().dump();
	}

	void PlayFromHere(const EditorTimeInfo& t, const std::string& chartName,
		const fs::path& gameRoot, const std::function<bool()>& saveToGame)
	{
		int bar = std::max(1, MeasureAt(t, t.now));
		LaunchTestPlay(gameRoot, chartName, StartSecForBar(t, bar), saveToGame);
	}

	void PlayFromTop(const std::string& chartName, const fs::path& gameRoot, const std::function<bool()>& saveToGame)
	{
		LaunchTestPlay(gameRoot, chartName, 0.0f, saveToGame);
	}

	// ============================================================
	// テストプレイのゲームの窓を、エディターのまんなかに重ねる（毎フレーム）
	//   x, y, w, h はエディターの中の座標（ImGui の座標）。画面の座標に直してから置く
	// ============================================================
	void UpdateEmbeddedGame(void* editorWindow, float x, float y, float w, float h)
	{
		HWND editor = static_cast<HWND>(editorWindow);
		if (!g_Proc.hProcess) { g_GameWnd = nullptr; return; }

		// ゲームが終わっていたら（Esc で閉じた等）後片付け
		DWORD code = 0;
		if (!GetExitCodeProcess(g_Proc.hProcess, &code) || code != STILL_ACTIVE)
		{
			CloseHandle(g_Proc.hProcess);
			CloseHandle(g_Proc.hThread);
			g_Proc = {};
			g_GameWnd = nullptr;
			g_LastRect = {};
			g_GameHidden = false;
			return;
		}

		if (!g_EmbedGame)
		{
			if (g_GameWnd) ReleaseGameWindow();
			return;
		}

		// まだ重ねていなければ、ゲームの窓を探す（読み込みが終わるまでは待つ）
		if (!g_GameWnd || !IsWindow(g_GameWnd))
		{
			g_GameWnd = nullptr;
			if (--g_FindCooldown > 0)
			{
				if (!IsIconic(editor)) DrawGameCover(x, y, w, h, false);	// 読み込み中
				return;
			}
			g_FindCooldown = 10;
			FindCtx f = { g_Proc.dwProcessId, nullptr };
			EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&f));
			if (!f.found || !IsGameReady(f.found))
			{
				if (!IsIconic(editor)) DrawGameCover(x, y, w, h, false);	// 読み込み中
				return;
			}

			g_GameWnd = f.found;
			g_GameStyle = GetWindowLongPtrW(g_GameWnd, GWL_STYLE);
			GetWindowRect(g_GameWnd, &g_GameRect);
			// 枠とタイトルを消す（子どもの窓にはしない）
			LONG_PTR style = g_GameStyle;
			style &= ~(LONG_PTR)(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			style |= WS_POPUP | WS_VISIBLE;
			SetWindowLongPtrW(g_GameWnd, GWL_STYLE, style);
			g_LastRect = {};
			g_GameHidden = false;
			SetForegroundWindow(g_GameWnd);		// すぐにキーで遊べるように、ゲームを前に
		}

		// エディターが最小化されている間は、ゲームも隠す
		if (IsIconic(editor))
		{
			if (!g_GameHidden) { ShowWindowAsync(g_GameWnd, SW_HIDE); g_GameHidden = true; }
			return;
		}
		if (g_GameHidden)
		{
			ShowWindowAsync(g_GameWnd, SW_SHOWNOACTIVATE);
			g_GameHidden = false;
			g_LastRect = {};
		}

		DrawGameCover(x, y, w, h, true);		// まんなかを黒でふさぐ（ゲームの窓はこの上に重なる）

		// 16:9 のまま、まんなかに収まる一番大きい大きさ
		float cw = w, ch = w * 9.0f / 16.0f;
		if (ch > h) { ch = h; cw = h * 16.0f / 9.0f; }
		if (cw < 16.0f || ch < 9.0f) return;
		POINT p = { (LONG)(x + (w - cw) * 0.5f), (LONG)(y + (h - ch) * 0.5f) };
		ClientToScreen(editor, &p);		// エディターの中の座標 → 画面の座標
		RECT r = { p.x, p.y, p.x + (LONG)cw, p.y + (LONG)ch };
		if (r.left != g_LastRect.left || r.top != g_LastRect.top || r.right != g_LastRect.right || r.bottom != g_LastRect.bottom)
		{
			// ASYNC：ゲームが忙しくても、エディターは待たない
			SetWindowPos(g_GameWnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);
			g_LastRect = r;
		}

		// エディターを触って前に来たら、ゲームをエディターのすぐ上に戻す（前に出すだけ。キーはエディターのまま）
		if (GetForegroundWindow() == editor && GetWindow(editor, GW_HWNDPREV) != g_GameWnd)
		{
			SetWindowPos(g_GameWnd, HWND_TOP, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
		}
	}

	// ============================================================
	// テストプレイのパネル
	// ============================================================
	void DrawTestPlayPanel(const EditorTimeInfo& t, const std::string& chartName,
		const fs::path& gameRoot, const std::function<bool()>& saveToGame)
	{
		ImGui::SeparatorText("Test Play (real game)");
		int bar = std::max(1, MeasureAt(t, t.now));
		float start = StartSecForBar(t, bar);

		if (ImGui::Button("Play from here", ImVec2(130, 0)))
			LaunchTestPlay(gameRoot, chartName, start, saveToGame);
		ImGui::SameLine();
		if (ImGui::Button("Play from top", ImVec2(110, 0)))
			LaunchTestPlay(gameRoot, chartName, 0.0f, saveToGame);
		ImGui::TextDisabled("here = bar %d - %d bar(s) -> %.2f s", bar, g_TestBarsBefore, start);

		ImGui::SetNextItemWidth(80);
		ImGui::InputInt("bars before", &g_TestBarsBefore);
		g_TestBarsBefore = std::clamp(g_TestBarsBefore, 0, 8);
		ImGui::SameLine();
		ImGui::Checkbox("Auto", &g_TestAuto);
		ImGui::SameLine();
		ImGui::Checkbox("Release exe", &g_TestRelease);
		ImGui::Checkbox("Show inside editor", &g_EmbedGame);		// ゲームの窓をエディターのまんなかに重ねる
		if (g_Proc.hProcess)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Close game")) CloseRunningGame();
		}

		if (!g_TestMessage.empty())
			ImGui::TextColored(g_TestIsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
				"%s", g_TestMessage.c_str());
	}

	// ============================================================
	// カメラのパネル
	// ============================================================
	void DrawCameraPanel(const EditorTimeInfo& t, const std::string& chartName,
		const fs::path& gameRoot, const std::function<bool()>& saveToGame)
	{
		ImGui::SeparatorText("Camera");
		ImGui::TextDisabled("The camera moves during 'blend' beats and arrives at the start of 'bar'.");

		static const char* kNames[kPresetCount + 1] = {};
		if (!kNames[0])
		{
			for (int i = 0; i < kPresetCount; i++) kNames[i] = kPresets[i].name;
			kNames[kCustomIndex] = "custom";
		}

		int removeAt = -1;
		for (int i = 0; i < (int)g_Camera.size(); i++)
		{
			EditorCameraKey& k = g_Camera[i];
			ImGui::PushID(9000 + i);

			ImGui::SetNextItemWidth(70);
			ImGui::InputInt("bar", &k.fromMeasure);
			k.fromMeasure = std::max(1, k.fromMeasure);

			ImGui::SameLine();
			ImGui::SetNextItemWidth(90);
			int idx = PresetIndex(k.preset);
			if (ImGui::Combo("##preset", &idx, kNames, kPresetCount + 1))
			{
				if (idx == kCustomIndex)
				{
					// custom にしたときは、今のカメラの値から始める（そこから少しずつ直せる）
					const PresetInfo& from = kPresets[PresetIndex(k.preset) % kPresetCount];
					std::copy(from.eye, from.eye + 3, k.eye);
					std::copy(from.target, from.target + 3, k.target);
				}
				k.preset = kNames[idx];
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::DragFloat("blend", &k.blendBeats, 0.1f, 0.0f, 16.0f, "%.1f");

			ImGui::SameLine();
			if (ImGui::SmallButton("Try"))		// 動き始める少し前から、本番のゲームで確かめる
			{
				int startBar = MeasureAt(t, MeasureStart(t, k.fromMeasure) - k.blendBeats * BeatSec(t));
				LaunchTestPlay(gameRoot, chartName, StartSecForBar(t, startBar), saveToGame);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) removeAt = i;

			if (k.preset == "custom")
			{
				ImGui::Indent();
				ImGui::DragFloat3("eye", k.eye, 5.0f, -5000.0f, 5000.0f, "%.0f");
				ImGui::DragFloat3("target", k.target, 5.0f, -5000.0f, 5000.0f, "%.0f");
				ImGui::Unindent();
			}
			ImGui::PopID();
		}
		if (removeAt >= 0) g_Camera.erase(g_Camera.begin() + removeAt);

		int curBar = std::max(1, MeasureAt(t, t.now));
		char label[48];
		sprintf_s(label, "+ Add at bar %d", curBar);
		if (ImGui::Button(label))
		{
			EditorCameraKey k;
			k.fromMeasure = curBar;
			k.preset = "far";
			k.blendBeats = 4.0f;
			g_Camera.push_back(k);
		}
		// 入力中に順番が入れ替わらないように、触っていない時だけ並べ直す
		if (!ImGui::IsAnyItemActive())
			std::stable_sort(g_Camera.begin(), g_Camera.end(),
				[](const EditorCameraKey& a, const EditorCameraKey& b) { return a.fromMeasure < b.fromMeasure; });
	}

	// ============================================================
	// タイムラインの印：動いている間は帯、着く小節の頭に線と名前
	// ============================================================
	void DrawCameraMarkers(ImDrawList* draw, float x0, float x1, float yTop, float yBottom,
		const EditorTimeInfo& t, const std::function<float(float)>& timeToY)
	{
		const ImU32 kLine = IM_COL32(200, 120, 255, 230);
		const ImU32 kBand = IM_COL32(170, 90, 255, 45);
		const ImU32 kText = IM_COL32(225, 190, 255, 255);
		for (const auto& k : g_Camera)
		{
			float tEnd = MeasureStart(t, k.fromMeasure);
			float tStart = tEnd - k.blendBeats * BeatSec(t);
			float yEnd = timeToY(tEnd);
			float yStart = timeToY(tStart);
			float yA = std::min(yEnd, yStart), yB = std::max(yEnd, yStart);
			if (yB < yTop || yA > yBottom) continue;

			draw->AddRectFilled(ImVec2(x0, std::max(yA, yTop)), ImVec2(x1, std::min(yB, yBottom)), kBand);
			if (yEnd >= yTop && yEnd <= yBottom)
			{
				draw->AddLine(ImVec2(x0, yEnd), ImVec2(x1, yEnd), kLine, 2.0f);
				std::string label = "CAM " + k.preset;
				ImVec2 size = ImGui::CalcTextSize(label.c_str());
				draw->AddText(ImVec2(x1 - size.x - 6.0f, yEnd - size.y - 2.0f), kText, label.c_str());
			}
		}
	}
}
