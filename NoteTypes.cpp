#include "NoteBehavior.h"
#include "ChartObject.h"
#include "TraceNote.h"

//新しいノーツはここに追加
void NoteRegistry::RegisterAll()
{
	static bool s_Done = false;	//2回目移行は何もしない
	if (s_Done) return;
	s_Done = true;

	Register((int)ChartNoteType::TRACE, std::make_unique<TraceNote>());
}