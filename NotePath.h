#pragma once
#include <SimpleMath.h>

struct ChartNote; // 完全な定義は ChartObject.h にある。ここでは const参照渡しのみなので前方宣言で足りる

// ============================================================
// NotePath : ノーツの位置補間だけを行う自由関数群。
//   ChartObjectの状態(メンバ変数)には一切依存しない純粋関数なので、
//   ここだけ切り出しても他のコードへの影響はない。
// ============================================================
namespace NotePath
{
	// TAP/FLICK用：spawnPos〜judgePosを [0,1] にクランプして補間（ベジェ対応）
	DirectX::SimpleMath::Vector3 EvaluateNotePosition(const ChartNote& note, float now);

	// LONGNOTE用：spawnPos〜judgePosを「クランプせず」直線補間
	//   （先端が判定ラインを越えても進み続けられるように）。
	//   note.spiralRadius が設定されていれば、進行方向を軸にした螺旋オフセットを加える。
	DirectX::SimpleMath::Vector3 EvaluateLongNotePosition(const ChartNote& note, float now);
}
