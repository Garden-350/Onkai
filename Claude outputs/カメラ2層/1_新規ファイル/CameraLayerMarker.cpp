#include "CameraLayerMarker.h"
#include "Renderer.h"

void CameraLayerMarker::Draw(Camera* cam)
{
	cam->SetLayer(m_Layer);
	if (m_ClearDepth) Renderer::ClearCurrentDepth();
	// ŽŸ‚É•`‚­ Object ‚ª GetViewMatrix() ‚ðŽg‚Á‚Ä‚àA³‚µ‚¢‘w‚Ìs—ñ‚É‚È‚é‚æ‚¤‚É‚·‚é
	cam->SetCamera(0);
}
