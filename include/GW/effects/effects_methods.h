#pragma once

#include "base/error_handling.h"

#include "GW/context/skill.h"

#include <atomic>
#include <cstdint>

namespace gw::effects {

using PostProcessEffectFn = void(__cdecl*)(uint32_t intensity, uint32_t tint);
using DropBuffFn = void(__cdecl*)(uint32_t buff_id);

uint32_t GetAlcoholLevel();
void GetDrunkAf(uint32_t intensity, uint32_t tint);

context::AgentEffectsArray* GetPartyEffectsArray();
context::AgentEffects* GetAgentEffectsArray(uint32_t agent_id);
context::AgentEffects* GetPlayerEffectsArray();
context::EffectArray* GetAgentEffects(uint32_t agent_id);
context::BuffArray* GetAgentBuffs(uint32_t agent_id);
context::EffectArray* GetPlayerEffects();
context::BuffArray* GetPlayerBuffs();
bool DropBuff(uint32_t buff_id);
context::Effect* GetPlayerEffectBySkillId(gw::constants::SkillID skill_id);
context::Buff* GetPlayerBuffBySkillId(gw::constants::SkillID skill_id);

extern PostProcessEffectFn g_post_process_effect_func;
extern PostProcessEffectFn g_post_process_effect_original;
extern DropBuffFn g_drop_buff_func;
extern std::atomic<uint32_t> g_alcohol_level;
extern std::atomic<bool> g_initialized;

}  // namespace gw::effects
