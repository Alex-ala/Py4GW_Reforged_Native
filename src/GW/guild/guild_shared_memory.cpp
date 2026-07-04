#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/guild.h"

namespace GW::shared_memory {

void RegisterGuildSegments(Manager& manager) {
    manager.SubscribeStruct<Context::GuildContext>("runtime.ctx.guild", &Context::GetGuildContext, /*enabled=*/true);

    manager.SubscribePointer("runtime.ptr.guild_array", [] { return reinterpret_cast<uintptr_t>(Context::GetGuildArray()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
