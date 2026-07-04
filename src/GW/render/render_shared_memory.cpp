#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/render.h"

namespace GW::shared_memory {

void RegisterRenderSegments(Manager& manager) {
    manager.SubscribeStruct<Context::GwDxContext>("runtime.ctx.render", &Context::GetRenderContext, /*enabled=*/true);
}

}  // namespace GW::shared_memory
