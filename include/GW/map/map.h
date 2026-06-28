#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/common/game_pos.h"
#include "GW/context/map.h"
#include "GW/context/pathing.h"

#include <atomic>
#include <cstdint>

namespace GW::map {

struct MapTypeInstanceInfo {
    uint32_t request_instance_map_type;
    bool is_outpost;
    Context::RegionType map_region_type;
};

struct MissionMapSubContext {
    uint32_t h0000[0xE];
};

struct MissionMapSubContext2 {
    uint32_t h0000;
    GW::Vec2f player_mission_map_pos;
    uint32_t h000c;
    GW::Vec2f mission_map_size;
    float unk;
    GW::Vec2f mission_map_pan_offset;
    GW::Vec2f mission_map_pan_offset2;
    float unk2[2];
    uint32_t unk3[9];
};
static_assert(sizeof(MissionMapSubContext2) == 0x58, "MissionMapSubContext2 size mismatch");

struct MissionMapContext {
    GW::Vec2f size;
    uint32_t h0008;
    GW::Vec2f last_mouse_location;
    uint32_t frame_id;
    GW::Vec2f player_mission_map_pos;
    GW::GWArray<MissionMapSubContext*> h0020;
    uint32_t h0030;
    uint32_t h0034;
    uint32_t h0038;
    MissionMapSubContext2* h003c;
    uint32_t h0040;
    uint32_t h0044;
};
static_assert(sizeof(MissionMapContext) == 0x48, "MissionMapContext size mismatch");

struct WorldMapContext {
    uint32_t frame_id;
    uint32_t h0004;
    uint32_t h0008;
    float h000c;
    float h0010;
    uint32_t h0014;
    float h0018;
    float h001c;
    float h0020;
    float h0024;
    float h0028;
    float h002c;
    float h0030;
    float h0034;
    float zoom;
    GW::Vec2f top_left;
    GW::Vec2f bottom_right;
    uint32_t h004c[7];
    float h0068;
    float h006c;
    uint32_t params[0x6D];
};
static_assert(sizeof(WorldMapContext) == 0x224, "WorldMapContext size mismatch");

bool Initialize();
void Shutdown();

using QueryAltitudeFn = int(__cdecl*)(
    const GamePos* point,
    float radius,
    float* altitude,
    Vec3f* terrain_normal);
using VoidFn = void(__cdecl*)();

// MissionMapContext* GetMissionMapContext();
// WorldMapContext* GetWorldMapContext();
// bool Travel(GW::Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number = 0, GW::Constants::Language language = static_cast<GW::Constants::Language>(0));
// bool Travel(GW::Constants::MapID map_id, GW::Constants::District district = static_cast<GW::Constants::District>(0), int district_number = 0);
// bool MapTestStart(uint32_t map_id, uint32_t alt_map_id, int number = 2, uint32_t count = 3, uint32_t delay_ms = 0, uint32_t timeout_ms = 10000, uint32_t message_id = 0x10000098);
// void MapTestStop();
// const char* MapTestGetStatus();
// bool MapTestIsActive();
// uint32_t MapTestGetCount();
// bool EnterChallenge();
// Deferred until UIMgr map-message plumbing exists.

int QueryAltitude(const GamePos& pos, float radius, float& altitude, Vec3f* terrain_normal = nullptr);
bool GetIsMapLoaded();
GW::Constants::MapID GetMapID();
bool GetIsMapUnlocked(GW::Constants::MapID map_id);
GW::Constants::ServerRegion GetRegion();
uintptr_t GetServerRegionPtr();
MapTypeInstanceInfo* GetMapTypeInstanceInfo(Context::RegionType map_type);
GW::Constants::Language GetLanguage();
bool GetIsObserving();
int GetDistrict();
uint32_t GetInstanceTime();
GW::Constants::InstanceType GetInstanceType();
GW::Constants::ServerRegion RegionFromDistrict(GW::Constants::District district);
GW::Constants::Language LanguageFromDistrict(GW::Constants::District district);
Context::MissionMapIconArray* GetMissionMapIconArray();
Context::PathingMapArray* GetPathingMap();
uint32_t GetFoesKilled();
uint32_t GetFoesToKill();
Context::AreaInfo* GetMapInfo(GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0));
uintptr_t GetInstanceInfoPtr();
inline Context::AreaInfo* GetCurrentMapInfo() {
    return GetMapInfo(GetMapID());
}
bool GetIsInCinematic();
bool SkipCinematic();
bool CancelEnterChallenge();

extern QueryAltitudeFn g_query_altitude_func;
extern VoidFn g_skip_cinematic_func;
extern VoidFn g_cancel_enter_challenge_mission_func;
extern GW::Constants::ServerRegion* g_region_id_addr;
extern Context::AreaInfo* g_area_info_addr;
extern MapTypeInstanceInfo* g_map_type_instance_infos;
extern uint32_t g_map_type_instance_infos_size;
extern uintptr_t g_instance_info_ptr;
extern std::atomic<bool> g_initialized;

}  // namespace GW::map
