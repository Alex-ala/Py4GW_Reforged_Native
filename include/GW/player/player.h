#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/context/player.h"
#include "GW/context/title.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace GW::player {

using PlayerNumber = uint32_t;

bool Initialize();
void Shutdown();

using RemoveActiveTitleFn = void(__cdecl*)();
using SetActiveTitleFn = void(__cdecl*)(uint32_t identifier);
using DepositFactionFn = void(__cdecl*)(uint32_t always_0, uint32_t allegiance, uint32_t amount);

bool SetActiveTitle(GW::Constants::TitleID title_id);
bool RemoveActiveTitle();

uint32_t GetPlayerAgentId(uint32_t player_id);
uint32_t GetAmountOfPlayersInInstance();

Context::PlayerArray* GetPlayerArray();
PlayerNumber GetPlayerNumber();

Context::Player* GetPlayerByID(uint32_t player_id = 0);
wchar_t* GetPlayerName(uint32_t player_id = 0);
wchar_t* SetPlayerName(uint32_t player_id, const wchar_t* replace_name);

// bool ChangeSecondProfession(GW::Constants::Profession profession, uint32_t hero_index = 0);
// Deferred until SkillbarMgr is migrated. Legacy body:
// return SkillbarMgr::ChangeSecondProfession(profession, hero_index);

Context::Player* GetPlayerByName(const wchar_t* name);

Context::Title* GetTitleTrack(GW::Constants::TitleID title_id);
GW::Constants::TitleID GetActiveTitleId();
Context::Title* GetActiveTitle();
std::vector<int> GetTitleIDs();
Context::TitleClientData* GetTitleData(GW::Constants::TitleID title_id);

bool DepositFaction(uint32_t allegiance);

extern RemoveActiveTitleFn g_remove_active_title_func;
extern SetActiveTitleFn g_set_active_title_func;
extern DepositFactionFn g_deposit_faction_func;
extern Context::TitleClientData* g_title_data;
extern std::atomic<bool> g_initialized;

}  // namespace GW::player
