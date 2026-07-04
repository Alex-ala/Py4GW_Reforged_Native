#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/party.h"
#include "GW/context/world.h"

// Party structs published to shared memory. One line per struct: comment a line
// to remove it (shifts layout), or pass false / call SetSegmentEnabled to
// publish zeros without shifting layout.
namespace GW::shared_memory {

void RegisterPartySegments(Manager& manager) {
    manager.SubscribeStruct<Context::PartyContext>("runtime.ctx.party", &Context::GetPartyContext, /*enabled=*/true);

    manager.SubscribePointer("runtime.ptr.player_array", [] { return reinterpret_cast<uintptr_t>(Context::GetPlayerArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.party_effects", [] { return reinterpret_cast<uintptr_t>(Context::GetPartyEffectsArray()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
