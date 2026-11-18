#include "pch.h"
#include "LatencyLab.h"
#include "LabAudio.h"
#include "EditorShell.h"
#include "imgui.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	// ---- 鳴らし方 ----
	const char* kBackendKeys[] = { "xaudio2", "adx", "wasapi" };			// 設定ファイルに書く名前
	const char* kBackendLabels[] = { "XAudio2", "ADX", u8"自作（WASAPI）" };
	int s_Backend = 0;
	int s_Opened = -1;							// 今作ってある鳴らし方
	std::unique_ptr<LabAudio> s_Audio;
	bool s_Focused = false;						// 窓を触っているか

	// ---- A：押して鳴らす ----
	double s_PressA = 0.0;
	bool s_WaitingA = false;					// 鳴り始めの時刻を待っている
	double s_LiveA = -1.0;						// 今の分かっている遅れ（ms）
	std::deque<double> s_ResultsA;				// 最近の結果（ms。-1 は測れなかった）
	double s_FlashUntil = 0.0;					// 四角を光らせる時刻まで

	// ---- B：合わせて押す ----
	float s_Bpm = 120.0f;
	constexpr int kPractice = 4;				// 最初の 4 拍は練習（数えない）
	constexpr int kCount = 32;					// 数える拍
	constexpr double kLead = 1.0;				// 最初のクリックまでの無音（秒）
	bool s_RunningB = false;
	double s_StartCallB = 0.0;					// PlayTrack を呼んだ時刻
	bool s_VisualB = false;						// 光でも拍を見せるか（オフ：音だけで合わせる）
	struct Tap { double clockMs; double syncedMs; bool hasSynced; };
	std::vector<Tap> s_Taps;

	// ---- 設定（ゲームに渡す） ----
	nlohmann::json s_Saved = nlohmann::json::object();	// 今保存されている物
	bool s_SavedLoaded = false;
	float s_OffsetEdit = 0.0f;					// 保存する値（B の結果。手で直せる）
	std::string s_Message;
	const char* kSettingsFile = "audio_settings.json";

	struct Stats { int n = 0; double mean = 0.0, sd = 0.0; };
	template <class F> Stats Calc(const std::vector<Tap>& taps, F get)
	{
		Stats s;
		std::vector<double> v;
		for (const auto& t : taps) { double x; if (get(t, x)) v.push_back(x); }
		s.n = (int)v.size();
		if (s.n == 0) return s;
		for (double x : v) s.mean += x;
		s.mean /= s.n;
		for (double x : v) s.sd += (x - s.mean) * (x - s.mean);
		s.sd = (s.n > 1) ? std::sqrt(s.sd / (s.n - 1)) : 0.0;
		return s;
	}

	void OpenBackend()
	{
		if (s_Opened == s_Backend && s_Audio) return;
		if (s_Audio) s_Audio->StopTrack();
		s_Audio.reset();
		switch (s_Backend)
		{
		case 0: s_Audio = CreateLabXAudio2(); break;
		case 1: s_Audio = CreateLabAdx(); break;
		case 2: s_Audio = CreateLabWasapi(); break;
		}
		s_Opened = s_Backend;
		s_WaitingA = false;
		s_RunningB = false;
		s_LiveA = -1.0;
	}

	void LoadSaved()
	{
		s_SavedLoaded = true;
		std::ifstream f(kSettingsFile);
		if (!f.is_open()) return;
		try { f >> s_Saved; }
		catch (...) { s_Saved = nlohmann::json::object(); }
		s_OffsetEdit = s_Saved.value("offsetMs", 0.0f);
		std::string b = s_Saved.value("backend", std::string("xaudio2"));
		for (int i = 0; i < 3; i++) if (b == kBackendKeys[i]) s_Backend = i;
	}

	void Save(const fs::path& gameRoot, const Stats& syncedStats, int taps, double sd)
	{
		nlohmann::json j;
		j["backend"] = kBackendKeys[s_Backend];
		j["offsetMs"] = s_OffsetEdit;					// ゲームの判定をずらす量（ゲームの時計で測ったずれ）
		if (syncedStats.n > 0) j["syncedOffsetMs"] = syncedStats.mean;	// 音に合わせた時計で測ったずれ（参考）
		j["taps"] = taps;
		j["spreadMs"] = sd;
		char date[32];
		std::time_t t = std::time(nullptr);
		std::tm tmv;
		localtime_s(&tmv, &t);
		std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &tmv);
		j["date"] = date;

		bool ok = true;
		{
			std::ofstream f(kSettingsFile);
			if (f.is_open()) f << j.dump(2); else ok = false;
		}
		std::error_code ec;
		if (fs::exists(gameRoot, ec))
		{
			std::ofstream f(gameRoot / kSettingsFile);
			if (f.is_open()) f << j.dump(2); else ok = false;
		}
		else ok = false;
		s_Saved = j;
		s_Message = ok ? u8"保存しました（エディターとゲームのフォルダ）" : u8"ゲームのフォルダに保存できませんでした";
	}

	// 光る四角（押してもよい）。押されたら true
	bool FlashBox(const char* id, float w, float h, bool lit)
	{
		ImVec2 p = ImGui::GetCursorScreenPos();
		bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
		ImDrawList* d = ImGui::GetWindowDrawList();
		d->AddRectFilled(p, ImVec2(p.x + w, p.y + h), lit ? IM_COL32(255, 255, 255, 255) : IM_COL32(35, 38, 48, 255), 6.0f);
		d->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(120, 130, 160, 255), 6.0f, 0, 2.0f);
		return pressed;
	}

	// ============================================================
	// A：押して鳴らす
	// ============================================================
	void DrawModeA(bool keyPressed)
	{
		ImGui::TextWrapped(u8"Space（またはこの四角）を押すと、四角が光って音が鳴ります。"
			u8"押してから「スピーカーから音が出た」とライブラリが言う時刻までを ms で出します。");
		double now = NowSec();
		bool press = FlashBox("##flashA", ImGui::GetContentRegionAvail().x, 110.0f, now < s_FlashUntil) || keyPressed;
		if (press && s_Audio && s_Audio->Ok())
		{
			if (s_WaitingA) { s_ResultsA.push_front(s_LiveA); }	// 前のがまだなら、分かっている所で確定
			s_PressA = NowSec();
			s_Audio->PlayClick();
			s_FlashUntil = s_PressA + 0.08;
			s_WaitingA = true;
			s_LiveA = -1.0;
		}
		// 鳴り始めの時刻を探す（XAudio2 は何回か見て一番早い物にするので、0.5 秒待ってから確定）
		if (s_WaitingA && s_Audio)
		{
			double heard = s_Audio->ClickHeardAt();
			if (heard > 0.0) s_LiveA = (heard - s_PressA) * 1000.0;
			if (NowSec() - s_PressA > 0.5)
			{
				s_ResultsA.push_front(s_LiveA);
				while (s_ResultsA.size() > 10) s_ResultsA.pop_back();
				s_WaitingA = false;
			}
		}

		// 結果
		double sum = 0.0; int n = 0; double mn = 1e9, mx = -1e9;
		for (double v : s_ResultsA) if (v >= 0.0) { sum += v; n++; mn = std::min(mn, v); mx = std::max(mx, v); }
		ImGui::Spacing();
		if (s_WaitingA) ImGui::Text(u8"今：%s", s_LiveA >= 0.0 ? (std::to_string((int)std::lround(s_LiveA)) + " ms").c_str() : u8"測っています…");
		else if (!s_ResultsA.empty()) ImGui::Text(u8"今：%s", s_ResultsA.front() >= 0.0 ? (std::to_string((int)std::lround(s_ResultsA.front())) + " ms").c_str() : u8"測れませんでした");
		if (n > 0)
		{
			ImGui::Text(u8"平均（最近 %d 回）：%.1f ms　（%.1f 〜 %.1f）", n, sum / n, mn, mx);
		}
		if (ImGui::SmallButton(u8"結果を消す")) s_ResultsA.clear();
		ImGui::TextDisabled(u8"※ 機械の遅れ（押した瞬間 → 音が出たはずの時刻）。キーボードの遅れと、耳までの距離は入っていない");
	}

	// ============================================================
	// B：合わせて押す
	// ============================================================
	void DrawModeB(bool keyPressed, const fs::path& gameRoot)
	{
		ImGui::TextWrapped(u8"「始める」を押すと 1 秒後からメトロノームが鳴ります（小節の頭は高い音）。"
			u8"音に合わせて Space を押してください。最初の 4 拍は練習で、そのあと 32 拍を数えます。");
		const double spb = 60.0 / s_Bpm;
		ImGui::SetNextItemWidth(120);
		ImGui::BeginDisabled(s_RunningB);
		ImGui::SliderFloat("BPM", &s_Bpm, 60.0f, 200.0f, "%.0f");
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox(u8"光でも見せる", &s_VisualB);
		ImGui::SetItemTooltip(u8"オフ：音だけで合わせる（音のずれを測るならオフ）\nオン：光にも合わせられる（光と音の両方のずれが混ざる）");

		double now = NowSec();
		if (!s_RunningB)
		{
			if (ImGui::Button(u8"始める", ImVec2(120, 0)) && s_Audio && s_Audio->Ok())
			{
				s_Taps.clear();
				s_Audio->PlayTrack(MakeMetronome(s_Audio->TrackRate(), s_Bpm, kPractice + kCount, (float)kLead));
				s_StartCallB = NowSec();
				s_RunningB = true;
			}
		}
		else
		{
			if (ImGui::Button(u8"やめる", ImVec2(120, 0))) { s_Audio->StopTrack(); s_RunningB = false; }
			double end = kLead + spb * (kPractice + kCount) + 0.6;
			if (now - s_StartCallB > end) { s_Audio->StopTrack(); s_RunningB = false; }
		}

		// 何拍目か（ゲームと同じ「鳴らせと言ってからの時計」で数える）
		double clockPos = now - s_StartCallB;
		int beatNow = (int)std::floor((clockPos - kLead) / spb);
		bool lit = false;
		if (s_RunningB && s_VisualB)
		{
			double sinceBeat = clockPos - (kLead + beatNow * spb);
			lit = (beatNow >= 0 && sinceBeat >= 0.0 && sinceBeat < 0.08);
		}
		bool press = FlashBox("##flashB", ImGui::GetContentRegionAvail().x, 70.0f, lit) || keyPressed;
		if (s_RunningB)
		{
			if (beatNow < kPractice) ImGui::Text(u8"練習：%d / %d", std::max(0, beatNow + 1), kPractice);
			else ImGui::Text(u8"%d / %d 拍", std::min(kCount, beatNow - kPractice + 1), kCount);
		}

		if (press && s_RunningB && s_Audio)
		{
			Tap tap = {};
			// ゲームの時計でのずれ：鳴らせと言ってからの時間で、一番近い拍と比べる
			int k = (int)std::lround((clockPos - kLead) / spb);
			tap.clockMs = (clockPos - (kLead + k * spb)) * 1000.0;
			// 音に合わせた時計でのずれ：ライブラリが言う「今聞こえている所」と比べる
			double heard = s_Audio->HeardTrackSec();
			if (heard >= 0.0)
			{
				int ks = (int)std::lround((heard - kLead) / spb);
				tap.syncedMs = (heard - (kLead + ks * spb)) * 1000.0;
				tap.hasSynced = true;
			}
			bool counted = (k >= kPractice && k < kPractice + kCount && std::fabs(tap.clockMs) < spb * 350.0);
			if (counted) s_Taps.push_back(tap);
		}

		// 結果
		Stats clock = Calc(s_Taps, [](const Tap& t, double& x) { x = t.clockMs; return true; });
		Stats synced = Calc(s_Taps, [](const Tap& t, double& x) { x = t.syncedMs; return t.hasSynced; });
		ImGui::Spacing();
		if (clock.n > 0)
		{
			ImGui::Text(u8"押した回数：%d", clock.n);
			ImGui::Text(u8"ゲームの時計でのずれ：%+.1f ms（ばらつき ±%.1f）", clock.mean, clock.sd);
			ImGui::SetItemTooltip(u8"「鳴らせ」と言った時からの時計で見たずれ。今のゲームはこの時計で判定しているので、これをゲームに使う\n＋：音より遅く押している（＝音が遅れて聞こえている）");
			if (synced.n > 0)
			{
				ImGui::Text(u8"音に合わせた時計でのずれ：%+.1f ms（±%.1f）", synced.mean, synced.sd);
				ImGui::SetItemTooltip(u8"ライブラリが言う「今聞こえている所」で見たずれ。ゲームの時刻を音に合わせる（ADX の GetTime など）ようにしたら、こちらが残るずれ");
			}
		}
		if (!s_RunningB && clock.n >= 8)
		{
			if (ImGui::Button(u8"この結果を入れる")) s_OffsetEdit = (float)clock.mean;
		}

		// ゲームに使う
		ImGui::SeparatorText(u8"ゲームに使う");
		ImGui::SetNextItemWidth(120);
		ImGui::InputFloat(u8"判定のずらし（ms）", &s_OffsetEdit, 1.0f, 5.0f, "%.1f");
		ImGui::SetItemTooltip(u8"ゲームは、押した時刻からこの ms を引いて判定する（＋なら遅めに押しても PERFECT になる）");
		ImGui::Text(u8"鳴らし方：%s", kBackendLabels[s_Backend]);
		if (ImGui::Button(u8"この設定をゲームに使う", ImVec2(200, 0)))
			Save(gameRoot, synced, clock.n, clock.sd);
		if (!s_Message.empty()) ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", s_Message.c_str());
		if (!s_Saved.empty())
		{
			std::string b = s_Saved.value("backend", std::string("?"));
			ImGui::TextDisabled(u8"今の設定：%s / %+.1f ms（%s）", b.c_str(), s_Saved.value("offsetMs", 0.0f),
				s_Saved.value("date", std::string("")).c_str());
		}
		ImGui::TextDisabled(u8"※ 今のゲームは XAudio2 で鳴っているので、今は XAudio2 で測った値を使う");
	}
}

namespace LatencyLab
{
	void DrawPanel(const fs::path& gameRoot)
	{
		s_Focused = false;
		if (EditorShell::BeginPanel(u8"音ズレ測定"))
		{
			if (!s_SavedLoaded) LoadSaved();
			s_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

			// 鳴らし方
			ImGui::BeginDisabled(s_RunningB);
			for (int i = 0; i < 3; i++)
			{
				if (i) ImGui::SameLine();
				ImGui::RadioButton(kBackendLabels[i], &s_Backend, i);
			}
			ImGui::EndDisabled();
			OpenBackend();
			if (s_Audio && !s_Audio->Ok())
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), u8"使えません：%s", s_Audio->Error().c_str());
			else if (s_Audio)
			{
				double rep = s_Audio->ReportedLatencyMs();
				if (rep >= 0.0) ImGui::TextDisabled(u8"ライブラリが言う出力の遅れ：%.1f ms", rep);
				else ImGui::TextDisabled(u8"ライブラリが言う出力の遅れ：（時刻の中で補正している）");
			}

			// Space は窓を触っている間だけ（文字を打っている間は効かせない）
			bool key = s_Focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false);

			if (ImGui::BeginTabBar("##labtabs"))
			{
				if (ImGui::BeginTabItem(u8"A 押して鳴らす")) { DrawModeA(key); ImGui::EndTabItem(); }
				if (ImGui::BeginTabItem(u8"B 合わせて押す")) { DrawModeB(key, gameRoot); ImGui::EndTabItem(); }
				ImGui::EndTabBar();
			}
		}
		EditorShell::EndPanel();
	}

	bool CapturesKeys() { return s_Focused; }

	void Shutdown()
	{
		if (s_Audio) s_Audio->StopTrack();
		s_Audio.reset();
		s_Opened = -1;
	}
}
