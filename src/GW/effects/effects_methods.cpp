#include "base/error_handling.h"

#include "GW/effects/effects_methods.h"

#include "GW/context/context.h"
#include "GW/context/world_context.h"

namespace gw::effects {

PostProcessEffectFn g_post_process_effect_func = nullptr;
PostProcessEffectFn g_post_process_effect_original = nullptr;
DropBuffFn g_drop_buff_func = nullptr;
std::atomic<uint32_t> g_alcohol_level = 0;
std::atomic<bool> g_initialized = false;

uint32_t GetAlcoholLevel() {
    return g_alcohol_level.load();
}

void GetDrunkAf(uint32_t intensity, uint32_t tint) {
    if (g_post_process_effect_original) {
        g_post_process_effect_original(intensity, tint);
    }
}

context::AgentEffectsArray* GetPartyEffectsArray() {
    context::WorldContext* world = context::GetWorldContext();
    return world && world->party_effects.valid() ? &world->party_effects : nullptr;
}

context::AgentEffects* GetAgentEffectsArray(uint32_t agent_id) {
    context::AgentEffectsArray* agent_effects = GetPartyEffectsArray();
    if (!agent_effects) {
        return nullptr;
    }

    for (auto& agent_effect : *agent_effects) {
        if (agent_effect.agent_id == agent_id) {
            return &agent_effect;
        }
    }
    return nullptr;
}

context::AgentEffects* GetPlayerEffectsArray() {
    return GetAgentEffectsArray(context::GetControlledCharacterId());
}

context::EffectArray* GetAgentEffects(uint32_t agent_id) {
    context::AgentEffects* effects = GetAgentEffectsArray(agent_id);
    return effects && effects->effects.valid() ? &effects->effects : nullptr;
}

context::BuffArray* GetAgentBuffs(uint32_t agent_id) {
    context::AgentEffects* effects = GetAgentEffectsArray(agent_id);
    return effects && effects->buffs.valid() ? &effects->buffs : nullptr;
}

context::EffectArray* GetPlayerEffects() {
    return GetAgentEffects(context::GetControlledCharacterId());
}

context::BuffArray* GetPlayerBuffs() {
    return GetAgentBuffs(context::GetControlledCharacterId());
}

bool DropBuff(uint32_t buff_id) {
    if (!g_drop_buff_func) {
        return false;
    }

    g_drop_buff_func(buff_id);
    return true;
}

context::Effect* GetPlayerEffectBySkillId(gw::constants::SkillID skill_id) {
    context::EffectArray* effects = GetPlayerEffects();
    if (!effects) {
        return nullptr;
    }

    for (auto& effect : *effects) {
        if (effect.skill_id == skill_id) {
            return &effect;
        }
    }
    return nullptr;
}

context::Buff* GetPlayerBuffBySkillId(gw::constants::SkillID skill_id) {
    context::BuffArray* buffs = GetPlayerBuffs();
    if (!buffs) {
        return nullptr;
    }

    for (auto& buff : *buffs) {
        if (buff.skill_id == skill_id) {
            return &buff;
        }
    }
    return nullptr;
}

}  // namespace gw::effects
