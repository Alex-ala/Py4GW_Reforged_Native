#pragma once

#include "base/error_handling.h"

#include <cstdint>

namespace GW::Context {

inline constexpr uint32_t kSharedMemoryVersion = 2;
inline constexpr size_t kSharedMemorySegmentNameCapacity = 64;

struct AccountContext;
struct AgentContext;
struct Camera;
struct CharContext;
struct Inventory;
struct MissionMapContext;
struct WorldMapContext;
struct FriendList;
struct GameplayContext;
struct GameContext;
struct GuildContext;
struct ItemContext;
struct MapContext;
struct PartyContext;
struct PreGameContext;
struct GwDxContext;
struct SalvageSessionInfo;
struct Skill;
struct AttributeInfo;
struct TradeContext;
struct WorldContext;
struct TextParser;

struct RuntimePointersSnapshot {
    uintptr_t mission_map_context = 0;
    uintptr_t world_map_context = 0;
    uintptr_t gameplay_context = 0;
    uintptr_t instance_info = 0;
    uintptr_t map_context = 0;
    uintptr_t game_context = 0;
    uintptr_t pregame_context = 0;
    uintptr_t world_context = 0;
    uintptr_t char_context = 0;
    uintptr_t agent_context = 0;
    uintptr_t guild_context = 0;
    uintptr_t party_context = 0;
    uintptr_t trade_context = 0;
    uintptr_t item_context = 0;
    uintptr_t friend_list = 0;
    uintptr_t render_context = 0;
    uintptr_t text_parser = 0;
    uintptr_t camera = 0;
    uintptr_t window_handle_ptr = 0;
};

bool Initialize();
void Shutdown();

GameContext* GetGameContext();
PreGameContext* GetPreGameContext();
WorldContext* GetWorldContext();
PartyContext* GetPartyContext();
CharContext* GetCharContext();
GuildContext* GetGuildContext();
ItemContext* GetItemContext();
AgentContext* GetAgentContext();
MapContext* GetMapContext();
AccountContext* GetAccountContext();
TradeContext* GetTradeContext();
GameplayContext* GetGameplayContext();
TextParser* GetTextParser();
Camera* GetCamera();
FriendList* GetFriendList();
MissionMapContext* GetMissionMapContext();
WorldMapContext* GetWorldMapContext();
SalvageSessionInfo* GetSalvageSessionInfo();
GwDxContext* GetRenderContext();
uintptr_t GetWindowHandlePtrAddress();
uint32_t GetControlledCharacterId();

}  // namespace GW::Context
