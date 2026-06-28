#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"
#include "GW/context/guild.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct GuildContext { // total: 0x3BC/956
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t h0014;
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ uint32_t h001C;
        /* +h0020 */ GW::GWArray<void*> h0020;
        /* +h0030 */ uint32_t h0030;
        /* +h0034 */ wchar_t player_name[20];
        /* +h005C */ uint32_t h005C;
        /* +h0060 */ uint32_t player_guild_index;
        /* +h0064 */ GHKey player_gh_key;
        /* +h0074 */ uint32_t h0074;
        /* +h0078 */ wchar_t announcement[256];
        /* +h0278 */ wchar_t announcement_author[20];
        /* +h02A0 */ uint32_t player_guild_rank;
        /* +h02A4 */ uint32_t h02A4;
        /* +h02A8 */ GW::GWArray<TownAlliance> factions_outpost_guilds;
        /* +h02B8 */ uint32_t kurzick_town_count;
        /* +h02BC */ uint32_t luxon_town_count;
        /* +h02C0 */ uint32_t h02C0;
        /* +h02C4 */ uint32_t h02C4;
        /* +h02C8 */ uint32_t h02C8;
        /* +h02CC */ GuildHistory player_guild_history;
        /* +h02DC */ uint32_t h02DC[7];
        /* +h02F8 */ GuildArray guilds;
        /* +h0308 */ uint32_t h0308[4];
        /* +h0318 */ GW::GWArray<void*> h0318;
        /* +h0328 */ uint32_t h0328;
        /* +h032C */ GW::GWArray<void*> h032C;
        /* +h033C */ uint32_t h033C[7];
        /* +h0358 */ GuildRoster player_roster;
        //... end of what i care about
    };

    static_assert(offsetof(GuildContext, player_name) == 0x34, "GuildContext::player_name offset mismatch");
    static_assert(offsetof(GuildContext, player_gh_key) == 0x64, "GuildContext::player_gh_key offset mismatch");
    static_assert(offsetof(GuildContext, announcement) == 0x78, "GuildContext::announcement offset mismatch");
    static_assert(offsetof(GuildContext, factions_outpost_guilds) == 0x2A8, "GuildContext::factions_outpost_guilds offset mismatch");
    static_assert(offsetof(GuildContext, guilds) == 0x2F8, "GuildContext::guilds offset mismatch");
    static_assert(offsetof(GuildContext, player_roster) == 0x358, "GuildContext::player_roster offset mismatch");

}  // namespace GW::Context
