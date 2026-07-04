#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/common/constants/constants.h"
#include "GW/context/agent.h"
#include "GW/context/context.h"
#include "GW/context/world.h"
#include "GW/map/map.h"

namespace GW::shared_memory {

namespace {

// Classified agent array: pointers + agent ids + per-category id-lists, rebuilt
// each frame. This is the one derived (non-game-struct) agent payload; the
// AgentContext itself is published verbatim below.
bool FillAgentArraySnapshot(Context::AgentArraySnapshot& snapshot, uint64_t) {
    snapshot = {};
    snapshot.max_size = Context::kSharedMemoryAgentArrayMaxSize;

    const auto instance_type = map::GetInstanceType();
    const bool is_map_ready = map::GetIsMapLoaded() &&
        !map::GetIsObserving() &&
        instance_type != Constants::InstanceType::Loading;
    if (!is_map_ready) {
        return false;
    }

    auto* agents = Context::GetAgentArray();
    const auto* agent_context = Context::GetAgentContext();
    if (!(agents && agent_context)) {
        return false;
    }

    auto push_ref = [](Context::AgentRefSnapshotArray& array, uint32_t agent_id, uint32_t index) {
        if (array.count >= Context::kSharedMemoryAgentArrayMaxSize) {
            return;
        }
        auto& entry = array.entries[array.count++];
        entry.agent_id = agent_id;
        entry.index = index;
    };

    for (const auto* agent : *agents) {
        if (!(agent && agent->agent_id)) {
            continue;
        }
        if (!(agent_context->agent_movement.size() > agent->agent_id &&
              agent_context->agent_movement[agent->agent_id])) {
            continue;
        }
        if (snapshot.count >= Context::kSharedMemoryAgentArrayMaxSize) {
            break;
        }

        const uint32_t slot = snapshot.count++;
        auto& out = snapshot.entries[slot];
        out.ptr = reinterpret_cast<uintptr_t>(agent);
        out.agent_id = agent->agent_id;

        push_ref(snapshot.all, agent->agent_id, slot);

        if (agent->GetIsGadgetType()) {
            push_ref(snapshot.gadget, agent->agent_id, slot);
            continue;
        }

        if (agent->GetIsItemType()) {
            const auto* item = agent->GetAsAgentItem();
            if (!item) {
                continue;
            }
            push_ref(snapshot.item, agent->agent_id, slot);
            if (item->owner != 0) {
                push_ref(snapshot.owned_item, agent->agent_id, slot);
            }
            continue;
        }

        const auto* living = agent->GetAsAgentLiving();
        if (!living) {
            continue;
        }

        push_ref(snapshot.living, agent->agent_id, slot);

        switch (living->allegiance) {
        case Constants::Allegiance::Ally_NonAttackable:
            push_ref(snapshot.ally, agent->agent_id, slot);
            if (living->GetIsDead()) {
                push_ref(snapshot.dead_ally, agent->agent_id, slot);
            }
            break;
        case Constants::Allegiance::Neutral:
            push_ref(snapshot.neutral, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Enemy:
            push_ref(snapshot.enemy, agent->agent_id, slot);
            if (living->GetIsDead()) {
                push_ref(snapshot.dead_enemy, agent->agent_id, slot);
            }
            break;
        case Constants::Allegiance::Spirit_Pet:
            push_ref(snapshot.spirit_pet, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Minion:
            push_ref(snapshot.minion, agent->agent_id, slot);
            break;
        case Constants::Allegiance::Npc_Minipet:
            push_ref(snapshot.npc_minipet, agent->agent_id, slot);
            break;
        default:
            break;
        }
    }

    return true;
}

}  // namespace

void RegisterAgentSegments(Manager& manager) {
    manager.SubscribeStruct<Context::AgentContext>("runtime.ctx.agent", &Context::GetAgentContext, /*enabled=*/true);
    manager.SubscribeSnapshot<Context::AgentArraySnapshot>("runtime.agents", &FillAgentArraySnapshot, /*enabled=*/true);

    // Dynamic agent-family lists: pointer only, materialized per-element in
    // Python (the agent array also has the classified snapshot above).
    manager.SubscribePointer("runtime.ptr.agent_array", [] { return reinterpret_cast<uintptr_t>(Context::GetAgentArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.map_agent_array", [] { return reinterpret_cast<uintptr_t>(Context::GetMapAgentArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.npc_array", [] { return reinterpret_cast<uintptr_t>(Context::GetNPCArray()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
