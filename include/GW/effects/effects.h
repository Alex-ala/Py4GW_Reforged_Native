#pragma once

#include "base/error_handling.h"

#include "GW/context/skill.h"

#include <atomic>
#include <cstdint>

namespace GW::effects {

bool Initialize();
void Shutdown();

using PostProcessEffectFn = void(__cdecl*)(uint32_t intensity, uint32_t tint);
using DropBuffFn = void(__cdecl*)(uint32_t buff_id);

uint32_t GetAlcoholLevel();
void GetDrunkAf(uint32_t intensity, uint32_t tint);

Context::AgentEffectsArray* GetPartyEffectsArray();
Context::AgentEffects* GetAgentEffectsArray(uint32_t agent_id);
Context::AgentEffects* GetPlayerEffectsArray();
Context::EffectArray* GetAgentEffects(uint32_t agent_id);
Context::BuffArray* GetAgentBuffs(uint32_t agent_id);
Context::EffectArray* GetPlayerEffects();
Context::BuffArray* GetPlayerBuffs();
bool DropBuff(uint32_t buff_id);
Context::Effect* GetPlayerEffectBySkillId(GW::Constants::SkillID skill_id);
Context::Buff* GetPlayerBuffBySkillId(GW::Constants::SkillID skill_id);

extern PostProcessEffectFn g_post_process_effect_func;
extern PostProcessEffectFn g_post_process_effect_original;
extern DropBuffFn g_drop_buff_func;
extern std::atomic<uint32_t> g_alcohol_level;
extern std::atomic<bool> g_initialized;

}  // namespace GW::effects
