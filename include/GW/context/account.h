#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/common/gw_array.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

struct AccountUnlockedCount {
    uint32_t id;
    uint32_t unk1;
    uint32_t unk2;
};
static_assert(sizeof(AccountUnlockedCount) == 0xC, "AccountUnlockedCount size mismatch");

// Entry of the account-wide available-characters list (the login/account roster,
// distinct from PreGameContext's login-screen preview buffer). Layout parity with
// legacy GW::AvailableCharacterInfo.
struct AvailableCharacterInfo {
    /* +h0000 */ uint32_t h0000[2];
    /* +h0008 */ uint32_t uuid[4];        // possibly pvp/campaign flags
    /* +h0018 */ wchar_t  player_name[20];
    /* +h0040 */ uint32_t props[17];      // packed map_id/profession/campaign/level/pvp bitfields

    GW::Constants::MapID map_id() const {
        return static_cast<GW::Constants::MapID>((props[0] >> 16) & 0xffff);
    }
    uint32_t primary() const { return (props[2] >> 20) & 0xf; }
    uint32_t secondary() const { return (props[7] >> 10) & 0xf; }
    uint32_t campaign() const { return props[7] & 0xf; }
    uint32_t level() const { return (props[7] >> 4) & 0x3f; }
    bool is_pvp() const { return ((props[7] >> 9) & 0x1) == 0x1; }
};
static_assert(sizeof(AvailableCharacterInfo) == 0x84, "AvailableCharacterInfo size mismatch");

struct AccountUnlockedItemInfo {
    uint32_t name_id;
    uint32_t mod_struct_index; // Used to find mod struct in unlocked_pvp_items_mod_structs...
    uint32_t mod_struct_size;
};
static_assert(sizeof(AccountUnlockedItemInfo) == 0xC, "AccountUnlockedItemInfo size mismatch");

struct AccountContext {
    /* +h0000 */ GWArray<AccountUnlockedCount> account_unlocked_counts; // e.g. number of unlocked storage panes
    /* +h0010 */ uint8_t h0010[0xA4];
    /* +h00b4 */ GWArray<uint32_t> unlocked_pvp_heros; // Unused, hero battles is no more :(
    /* +h00c4 */ GWArray<uint32_t> h00c4;// If an item is unlocked, the mod struct is stored here. Use unlocked_pvp_items_info to find the index. Idk why, chaos reigns I guess
    /* +h00e4 */ GWArray<AccountUnlockedItemInfo> unlocked_pvp_item_info; // If an item is unlocked, the details are stored here
    /* +h00f4 */ GWArray<uint32_t> unlocked_pvp_items; // Bitwise array of which pvp items are unlocked
    /* +h0104 */ uint8_t h0104[0x30]; // Some arrays, some linked lists, meh
    /* +h0124 */ GWArray<uint32_t> unlocked_account_skills; // List of skills unlocked (but not learnt) for this account, i.e. skills that heros can use, tomes can unlock
    /* +h0134 */ uint32_t account_flags;
};
static_assert(sizeof(AccountContext) == 0x138, "AccountContext size mismatch");

}  // namespace GW::Context
