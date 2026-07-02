#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/memory_patcher.h"
#include "base/patterns.h"

#include "GW/map/map.h"

namespace GW::Context {
extern GW::Constants::ServerRegion* g_region_id_addr;
extern AreaInfo* g_area_info_addr;
extern MapTypeInstanceInfo* g_map_type_instance_infos;
extern uint32_t g_map_type_instance_infos_size;
extern uintptr_t g_instance_info_ptr;
extern InstanceInfo* g_instance_info;
}

namespace GW::map {

using QueryAltitudeFn = int(__cdecl*)(const GamePos* point, float radius, float* altitude, Vec3f* terrain_normal);
using VoidFn = void(__cdecl*)();
using DoActionFn = void(__cdecl*)(uint32_t);

extern QueryAltitudeFn g_query_altitude_func;
extern VoidFn g_skip_cinematic_func;
extern VoidFn g_cancel_enter_challenge_mission_func;
extern DoActionFn g_enter_challenge_mission_func;
extern PY4GW::MemoryPatcher g_bypass_tolerance_patch;

bool ResolveSkipCinematic() {
    CrashContextScope context("startup", "map", "resolve_skip_cinematic");
    return PY4GW::Patterns::Resolve("map.skip_cinematic_func", &g_skip_cinematic_func);
}

bool ResolveRegionId() {
    CrashContextScope context("startup", "map", "resolve_region_id");
    return PY4GW::Patterns::Resolve("map.region_id_addr", &Context::g_region_id_addr);
}

bool ResolveAreaInfo() {
    CrashContextScope context("startup", "map", "resolve_area_info");
    return PY4GW::Patterns::Resolve("map.area_info_addr", &Context::g_area_info_addr);
}

bool ResolveInstanceInfo() {
    CrashContextScope context("startup", "map", "resolve_instance_info");
    Context::g_instance_info_ptr = 0;
    return PY4GW::Patterns::Resolve("map.instance_info_addr", &Context::g_instance_info) &&
        PY4GW::Patterns::Resolve("map.instance_info_ptr_ref", &Context::g_instance_info_ptr);
}

bool ResolveQueryAltitude() {
    CrashContextScope context("startup", "map", "resolve_query_altitude");
    return PY4GW::Patterns::Resolve("map.query_altitude_func", &g_query_altitude_func);
}

bool ResolveBypassTolerancePatch() {
    CrashContextScope context("startup", "map", "resolve_bypass_tolerance_patch");
    uintptr_t address = 0;
    if (!PY4GW::Patterns::Resolve("map.bypass_tolerance_patch_addr", &address)) {
        return false;
    }

    static constexpr char patch[] = "\xEB";
    g_bypass_tolerance_patch.SetPatch(address, patch, 1);
    return g_bypass_tolerance_patch.IsValid();
}

bool ResolveEnterChallengeFunctions() {
    CrashContextScope context("startup", "map", "resolve_enter_challenge_functions");
    return PY4GW::Patterns::Resolve("map.cancel_enter_challenge_mission_func", &g_cancel_enter_challenge_mission_func) &&
        PY4GW::Patterns::Resolve("map.enter_challenge_mission_func", &g_enter_challenge_mission_func);
}

bool ResolveMapTypeInstanceInfos() {
    CrashContextScope context("startup", "map", "resolve_map_type_instance_infos");
    return PY4GW::Patterns::Resolve("map.map_type_instance_infos_size", &Context::g_map_type_instance_infos_size) &&
        PY4GW::Patterns::Resolve("map.map_type_instance_infos_ptr", &Context::g_map_type_instance_infos);
}

// Raw raycast bridge symbols (PyMap raycast, ported from legacy py_map.cpp).
// Resolved lazily on first use from the bindings, parity with the legacy
// Ensure* helpers; byte patterns and assertion strings live in offsets/map.json.
using MapIntersectFn = uint32_t(__cdecl*)(Vec3f* origin, Vec3f* unit_direction, Vec3f* hit_point, int* prop_layer);
using TerrainQueryIntersectionFn = uint32_t(__cdecl*)(void* terrain, Vec3f* src, Vec3f* dest, float unk, float* dist);
using GrModelIntersectConeFn = uint32_t(__cdecl*)(void* model, Vec3f* origin, Vec3f* unit_dir, float cone, float* distance, int flag);

MapIntersectFn g_map_intersect_func = nullptr;
TerrainQueryIntersectionFn g_terrain_query_intersection_func = nullptr;
GrModelIntersectConeFn g_gr_model_intersect_cone_func = nullptr;

// MapCliQueryIntersection: terrain + walkable-prop collision. Tries the disp32
// anchor first (EXE build 20-5-2026), then the older disp8 encoding; both sit
// inside the function and to_function_start walks back to the prologue.
bool ResolveMapIntersectFunction() {
    if (g_map_intersect_func) {
        return true;
    }
    CrashContextScope context("runtime", "map", "resolve_map_cli_query_intersection");
    PY4GW::Patterns::Resolve("map.map_cli_query_intersection_func", &g_map_intersect_func);
    return g_map_intersect_func != nullptr;
}

// ITerrain::QueryIntersection: terrain-only test used by the reference HasLos.
bool ResolveTerrainQueryIntersection() {
    if (g_terrain_query_intersection_func) {
        return true;
    }
    CrashContextScope context("runtime", "map", "resolve_terrain_query_intersection");
    PY4GW::Patterns::Resolve("map.terrain_query_intersection_func", &g_terrain_query_intersection_func);
    return g_terrain_query_intersection_func != nullptr;
}

// CIGrModel::IntersectCone: per-geoset interactive-prop mesh test, resolved by
// the stable "Invalid unit vector" assertion anchor in GrModel.cpp.
bool ResolveGrModelIntersectCone() {
    if (g_gr_model_intersect_cone_func) {
        return true;
    }
    CrashContextScope context("runtime", "map", "resolve_gr_model_intersect_cone");
    PY4GW::Patterns::Resolve("map.gr_model_intersect_cone_func", &g_gr_model_intersect_cone_func);
    return g_gr_model_intersect_cone_func != nullptr;
}

}  // namespace GW::map
