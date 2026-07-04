#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/camera.h"
#include "GW/context/context.h"

namespace GW::shared_memory {

void RegisterCameraSegments(Manager& manager) {
    manager.SubscribeStruct<Context::Camera>("runtime.ctx.camera", &Context::GetCamera, /*enabled=*/true);
}

}  // namespace GW::shared_memory
