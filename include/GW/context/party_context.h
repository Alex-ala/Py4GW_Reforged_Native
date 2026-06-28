#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"
#include "GW/common/gw_list.h"
#include "GW/context/party.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct PartyContext { // total: 0x58/88
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ GW::GWArray<void*> h0004;
        /* +h0014 */ uint32_t flag;
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ GW::GwList<PartyInfo> requests;
        /* +h0028 */ uint32_t requests_count;
        /* +h002C */ GW::GwList<PartyInfo> sending;
        /* +h0038 */ uint32_t sending_count;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ GW::GWArray<PartyInfo*> parties;
        /* +h0050 */ uint32_t h0050;
        /* +h0054 */ PartyInfo* player_party; // Players party
        /* +h0058 */ uint8_t h0058[104];
        /* +h00C0 */ GW::GWArray<PartySearch*> party_search;

        bool InHardMode() const { return (flag & 0x10) > 0; }
        bool IsDefeated() const { return (flag & 0x20) > 0; }
        bool IsPartyLeader() const { return (flag >> 0x7) & 1; }
    };

static_assert(offsetof(PartyContext, requests) == 0x1C, "PartyContext::requests offset mismatch");
static_assert(offsetof(PartyContext, parties) == 0x40, "PartyContext::parties offset mismatch");
static_assert(offsetof(PartyContext, player_party) == 0x54, "PartyContext::player_party offset mismatch");
static_assert(offsetof(PartyContext, party_search) == 0xC0, "PartyContext::party_search offset mismatch");
static_assert(sizeof(PartyContext) == 0xD0, "PartyContext size mismatch");

}  // namespace GW::Context
