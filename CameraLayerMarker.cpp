#include "CameraLayerMarker.h"
#include "Renderer.h"

void CameraLayerMarker::Draw(Camera* cam)
{
	cam->SetLayer(m_Layer);
	if (m_ClearDepth) Renderer::ClearCurrentDepth();
	cam->SetCamera(0);
}
