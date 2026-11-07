#pragma once
#include "NoteBehavior.h"
#include "NoteMesh.h"

//トレースノーツ：押してたら取れる
class TraceNote : public NoteBehavior
{
public:
	static constexpr float kLateWindow = 0.1f;	//判定の時刻を過ぎてから何秒まで押し始めても取れるかの時間

	const char* Name() const override { return "TRACE"; }
	bool Draw(const ChartNote& note, const NoteDrawContext& ctx) override;
	void Judge(ChartNote& note, const NoteJudgeContext& ctx) override;
	NoteLook Look(const ChartNote& note) const override;

private:
	NoteMesh m_Mesh;
	bool m_TriedLoad = false;	//読み込みをためしたかどうか　失敗しても毎フレーム読まないためのトリガー
};

