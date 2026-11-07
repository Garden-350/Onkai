#pragma once
#include <map>
#include <memory>
#include <SimpleMath.h>

struct ChartNote;
class JudgeSystem;
class Sound;

//新しいノーツを足すための仕組み
//NoteBehavior:1種類のノーツの「判定の仕方」「見た目」を設定
//NoteRegistry:種類の番号(譜面の"type")
//新しいノーツを設定する際は「NoteBehavior」を継承したクラスを1つ作ってNoteTypes.cppに登録

//判定に使用　chartObjectが毎フレーム渡す
struct NoteJudgeContext
{
	float musicTime = 0.0f;	//曲の再生時間
	bool autoMode = false;
	JudgeSystem* judge = nullptr;	//判定の結果を記録するところ
	Sound* sound = nullptr;
	bool(*isLaneHeld)(int lane) = nullptr;	//そのレーンのキーを今押しているか
};

//描くときに使う
struct NoteDrawContext
{
	DirectX::SimpleMath::Vector3 pos;	//描画位置
	DirectX::SimpleMath::Vector3 color;	//描画色
	float scale = 1.0f;
	float musicTime = 0.0f;	//曲の時間　アニメーションとかあったら使用
};

//見た目　現在はタップのモデルを拝借してるだけ
struct NoteLook
{
	float scale = 1.0f;	//タップに対する大きさ
	float whiteMix = 0.0f;	//白をどれだけ混ぜるか（0 = レーンの色そのまま、1 = 真っ白）
	bool approachRing = true;	//近づいてくるときのリングを出すかどうか
};

class NoteBehavior
{
public:
	virtual ~NoteBehavior() = default;
	virtual const char* Name() const = 0;	//譜面の"type"に対応する文字列
	virtual void Judge(ChartNote& note, const NoteJudgeContext& context) = 0;	//判定処理
	virtual bool Draw(const ChartNote& note, const NoteDrawContext& ctx) { return false; }
	virtual NoteLook Look(const ChartNote& note) const { return NoteLook(); }	//見た目 なにも書かなければタップと同じ
};

class NoteRegistry
{
public:
	static void Register(int type, std::unique_ptr<NoteBehavior> behavior) { Table()[type] = std::move(behavior); }

	//登録されていなければnullptr
	static NoteBehavior* Find(int type)
	{
		auto it = Table().find(type);
		return(it == Table().end()) ? nullptr : it->second.get();
	}
	static void RegisterAll();	//全部の種類を登録する関数（NoteTypes.cppで定義）

private:
	//関数の中のstatic:初めて呼ばれたときに作られ、ゲームを閉じるまで生存
	static std::map<int, std::unique_ptr<NoteBehavior>>& Table()
	{
		static std::map<int, std::unique_ptr<NoteBehavior>> table;
		return table;
	}
};
