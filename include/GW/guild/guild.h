#pragma once

#include "base/error_handling.h"

#include "GW/context/guild.h"

#include <atomic>
#include <cstdint>

namespace GW::guild {

bool Initialize();
void Shutdown();

Context::GuildArray* GetGuildArray();
Context::Guild* GetPlayerGuild();
Context::Guild* GetCurrentGH();
Context::Guild* GetGuildInfo(uint32_t guild_id);
uint32_t GetPlayerGuildIndex();
wchar_t* GetPlayerGuildAnnouncement();
wchar_t* GetPlayerGuildAnnouncer();

bool TravelGH();
bool TravelGH(Context::GHKey key);
bool LeaveGH();

extern std::atomic<bool> g_initialized;

}  // namespace GW::guild
