#pragma once

namespace Math { class Camera; }


namespace SkyPass
{
    void Initialize(void);
    void Shutdown(void);
    void Render(GraphicsContext& Context, const Math::Camera& camera);
}
