#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"

#include <cstdint>

namespace GW::Context {

using PlayerID = uint32_t;

    struct Player { // total: 0x4C/76
        /* +h0000 */ uint32_t agent_id;
        /* +h0004 */ uint32_t h0004[3];
        /* +h0010 */ uint32_t appearance_bitmap;
        /* +h0014 */ uint32_t flags; // Bitwise field
        /* +h0018 */ uint32_t primary;
        /* +h001C */ uint32_t secondary;
        /* +h0020 */ uint32_t h0020;
        /* +h0024 */ wchar_t* name_enc;
        /* +h0028 */ wchar_t* name;
        /* +h002C */ uint32_t party_leader_player_number;
        /* +h0030 */ uint32_t active_title_tier;
        /* +h0034 */ uint32_t reforged_or_dhuums_flags;
        /* +h0038 */ uint32_t player_number;
        /* +h003C */ uint32_t party_size;
        /* +h0040 */ GW::GWArray<void*> h0040;

        inline bool IsPvP() {
            return (flags & 0x800) != 0;
        }

    };
static_assert(sizeof(Player) == 0x50, "Player size mismatch");

using PlayerArray = GW::GWArray<Player>;

}  // namespace GW::Context
