#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/map.h"
#include "GW/map/map.h"

#include <cstring>

namespace GW::shared_memory {

namespace {

// Flattened map summary: scalar counts + the map's own pointers, so a reader
// gets map extents and collection sizes without walking the context. The raw
// MapContext is also published verbatim below.
bool FillMapContextSnapshot(Context::MapContextSnapshot& snapshot, uint64_t) {
    snapshot = {};

    const auto* map_context = Context::GetMapContext();
    if (!map_context) {
        return false;
    }

    std::memcpy(snapshot.map_boundaries, map_context->map_boundaries, sizeof(snapshot.map_boundaries));
    std::memcpy(snapshot.trapezoid_bounds, map_context->h005C, sizeof(snapshot.trapezoid_bounds));

    snapshot.terrain = reinterpret_cast<uintptr_t>(map_context->terrain);
    snapshot.zones = reinterpret_cast<uintptr_t>(map_context->zones);
    snapshot.props = reinterpret_cast<uintptr_t>(map_context->props);
    snapshot.pathing_sub1 = reinterpret_cast<uintptr_t>(map_context->sub1);

    if (map_context->sub1) {
        snapshot.pathing_sub2 = reinterpret_cast<uintptr_t>(map_context->sub1->sub2);
        snapshot.pathing_block_count = static_cast<uint32_t>(map_context->sub1->pathing_map_block.size());
        snapshot.total_trapezoid_count = map_context->sub1->total_trapezoid_count;
        if (map_context->sub1->sub2) {
            snapshot.pathing_map_count = static_cast<uint32_t>(map_context->sub1->sub2->pmaps.size());
        }
    }

    if (map_context->props) {
        snapshot.prop_model_count = static_cast<uint32_t>(map_context->props->propModels.size());
        snapshot.prop_array_count = static_cast<uint32_t>(map_context->props->propArray.size());
    }

    snapshot.instance_info = Context::GetInstanceInfoPtr();
    if (const auto* instance_info = Context::GetInstanceInfo()) {
        snapshot.instance_type = static_cast<uint32_t>(instance_info->instance_type);
        snapshot.current_map_info = reinterpret_cast<uintptr_t>(instance_info->current_map_info);
        snapshot.terrain_count = instance_info->terrain_count;
    }

    return true;
}

}  // namespace

void RegisterMapSegments(Manager& manager) {
    manager.SubscribeStruct<Context::MapContext>("runtime.ctx.map", &Context::GetMapContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::MissionMapContext>("runtime.ctx.mission_map", &Context::GetMissionMapContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::WorldMapContext>("runtime.ctx.world_map", &Context::GetWorldMapContext, /*enabled=*/true);
    manager.SubscribeSnapshot<Context::MapContextSnapshot>("runtime.map.summary", &FillMapContextSnapshot, /*enabled=*/true);

    manager.SubscribePointer("runtime.ptr.area_info_array", [] { return reinterpret_cast<uintptr_t>(Context::GetAreaInfoArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.map_type_instance_infos", [] { return reinterpret_cast<uintptr_t>(Context::GetMapTypeInstanceInfos()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.pathing_map", [] { return reinterpret_cast<uintptr_t>(GW::map::GetPathingMap()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.instance_info", [] { return reinterpret_cast<uintptr_t>(Context::GetInstanceInfo()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.region_id", [] { return reinterpret_cast<uintptr_t>(Context::GetRegionIdPtr()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
