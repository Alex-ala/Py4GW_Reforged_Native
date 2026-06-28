#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct AgentMovement;

    struct AgentSummaryInfo {
        struct AgentSummaryInfoSub {
            /* +h0000 */ uint32_t h0000;
            /* +h0004 */ uint32_t h0004;
            /* +h0008 */ uint32_t gadget_id;
            /* +h000C */ uint32_t h000C;
            /* +h0010 */ wchar_t* gadget_name_enc;
            /* +h0014 */ uint32_t h0014;
            /* +h0018 */ uint32_t composite_agent_id; // 0x30000000 | player_id, 0x20000000 | npc_id etc
        };

        uint32_t h0000;
        uint32_t h0004;
        AgentSummaryInfoSub* extra_info_sub;
    };
    static_assert(sizeof(AgentSummaryInfo) == 0xC, "AgentSummaryInfo size mismatch");

    struct AgentContext {
        /* +h0000 */ GWArray<void*> h0000;
        /* +h0010 */ uint32_t h0010[5];
        /* +h0024 */ uint32_t h0024; // function
        /* +h0028 */ uint32_t h0028[2];
        /* +h0030 */ uint32_t h0030; // function
        /* +h0034 */ uint32_t h0034[2];
        /* +h003C */ uint32_t h003C; // function
        /* +h0040 */ uint32_t h0040[2];
        /* +h0048 */ uint32_t h0048; // function
        /* +h004C */ uint32_t h004C[2];
        /* +h0054 */ uint32_t h0054; // function
        /* +h0058 */ uint32_t h0058[11];
        /* +h0084 */ GWArray<void*> h0084;
        /* +h0094 */   uint32_t h0094; // this field and the next array are link together in a structure.
        /* +h0098 */   GWArray<AgentSummaryInfo> agent_summary_info; // elements are of size 12. {ptr, func, ptr}
        /* +h00A8 */ GWArray<void*> h00A8;
        /* +h00B8 */ GWArray<void*> h00B8;
        /* +h00C8 */ uint32_t rand1; // Number seems to be randomized quite a bit o.o seems to be accessed by textparser.cpp
        /* +h00CC */ uint32_t rand2;
        /* +h00D0 */ uint8_t h00D0[24];
        /* +h00E8 */ GWArray<AgentMovement*> agent_movement;
        /* +h00F8 */ GWArray<void*> h00F8;
        /* +h0108 */ uint32_t h0108[0x11];
        /* +h014C */ GWArray<void*> agent_array1;
        /* +h015C */ GWArray<void*> agent_async_movement;
        /* +h016C */ uint32_t h016C[0x10];
        /* +h01AC */ uint32_t instance_timer;
        //... more but meh
    };

static_assert(offsetof(AgentContext, agent_summary_info) == 0x98, "AgentContext::agent_summary_info offset mismatch");
static_assert(offsetof(AgentContext, agent_movement) == 0xE8, "AgentContext::agent_movement offset mismatch");
static_assert(offsetof(AgentContext, agent_array1) == 0x14C, "AgentContext::agent_array1 offset mismatch");
static_assert(offsetof(AgentContext, agent_async_movement) == 0x15C, "AgentContext::agent_async_movement offset mismatch");
static_assert(offsetof(AgentContext, instance_timer) == 0x1AC, "AgentContext::instance_timer offset mismatch");

}  // namespace GW::Context
