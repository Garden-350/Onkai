#include "TraceNote.h"
#include "ChartObject.h"
#include "JudgeSystem.h"

using namespace DirectX::SimpleMath;

bool TraceNote::Draw(const ChartNote& note, const NoteDrawContext& ctx)
{
	//‰‚ß‚Ä•`‚­‚Æ‚«‚É1‰ñ‚¾‚¯“Ç‚İ‚İ
	if (!m_TriedLoad)
	{
		m_TriedLoad = true;
		if (m_Mesh.Load("assets/model/note/trace.fbx", "assets/model/note"))
			m_Mesh.SetTexture("assets/model/note/note_trace.png");
	}
	if (!m_Mesh.IsLoaded()) return false;	//“Ç‚ß‚È‚©‚Á‚½‚ç‚±‚ê‚ğ•Ô‚·

	Matrix world = Matrix::CreateScale(ctx.scale)
		* Matrix::CreateTranslation(ctx.pos);
	m_Mesh.Draw(world, ctx.color);

	return true;
}

void TraceNote::Judge(ChartNote& note, const NoteJudgeContext& ctx)
{
	float d = ctx.musicTime - note.time;	//”»’è‚Ì‚©‚ç‚Ç‚ê‚¾‚¯‚·‚¬‚½‚©
	if (d < 0.0f) return;

	bool held = ctx.autoMode || (ctx.isLaneHeld && ctx.isLaneHeld(note.lane));
	if (held)
	{
		ctx.judge->AddHit(note, 0, *ctx.sound);	//‰Ÿ‚µ‚Ä‚¢‚½‚Æ‚«
	}
	else
	{
		ctx.judge->AddMiss(note);
	}

}

NoteLook TraceNote::Look(const ChartNote& note) const
{
	NoteLook look;
	look.scale = 1.5f;
	look.whiteMix = 0.0f;
	look.approachRing = false;
	return look;
}
