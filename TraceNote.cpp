#include "TraceNote.h"
#include "ChartObject.h"
#include "JudgeSystem.h"

using namespace DirectX::SimpleMath;

bool TraceNote::Draw(const ChartNote& note, const NoteDrawContext& ctx)
{
	//èâÇﬂÇƒï`Ç≠Ç∆Ç´Ç…1âÒÇæÇØì«Ç›çûÇ›
	if (!m_TriedLoad)
	{
		m_TriedLoad = true;
		if (m_Mesh.Load("assets/model/note/trace.fbx", "assets/model/note"))
			m_Mesh.SetTexture("assets/model/note/note_trace.png");
	}
	if (!m_Mesh.IsLoaded()) return false;	//ì«ÇﬂÇ»Ç©Ç¡ÇΩÇÁÇ±ÇÍÇï‘Ç∑

	Matrix world = Matrix::CreateScale(ctx.scale)
		* ctx.rot
		* Matrix::CreateTranslation(ctx.pos);

	const Vector3 kTraceColor(1.0f, 0.85f, 0.5f);
	LIGHT saved = Renderer::GetLight();
	LIGHT dark = saved;
	dark.Diffuse = Color(0.0f, 0.0f, 0.0f, 1.0f);
	dark.Ambient = Color(0.0f, 0.0f, 0.0f, 1.0f);
	Renderer::SetLight(dark);

	m_Mesh.Draw(world, kTraceColor);
	Renderer::SetLight(saved);	//ñﬂÇ∑

	return true;
}

void TraceNote::Judge(ChartNote& note, const NoteJudgeContext& ctx)
{
	float d = ctx.musicTime - note.time;	//îªíËÇÃéûçèÇ©ÇÁÇ«ÇÍÇæÇØÇ∑Ç¨ÇΩÇ©
	if (d < 0.0f) return;

	bool held = ctx.autoMode || (ctx.isLaneHeld && ctx.isLaneHeld(note.lane));
	if (held)
	{
		ctx.judge->AddHit(note, 0, *ctx.sound);	//âüÇµÇƒÇ¢ÇΩÇ∆Ç´
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
