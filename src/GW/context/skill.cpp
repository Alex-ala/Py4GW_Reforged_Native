#include "base/error_handling.h"

#include "GW/context/skill.h"

#include "base/memory_manager.h"

#include <iterator>

namespace GW::Context {

bool Skill::IsUnused() const {
    for (auto unused_skill_id : GW::Constants::unused_skill_ids) {
        if (unused_skill_id == skill_id) {
            return true;
        }
    }
    return false;
}

uint32_t SkillbarSkill::GetRecharge() const {
    if (recharge == 0) {
        return 0;
    }
    return recharge - PY4GW::MemoryManager::GetSkillTimer();
}

SkillbarSkill* Skillbar::GetSkillById(GW::Constants::SkillID query_skill_id, size_t* slot_out) {
    for (size_t i = 0; i < std::size(skills); ++i) {
        if (skills[i].skill_id == query_skill_id) {
            if (slot_out) {
                *slot_out = i;
            }
            return &skills[i];
        }
    }
    return nullptr;
}

DWORD Effect::GetTimeElapsed() const {
    return PY4GW::MemoryManager::GetSkillTimer() - timestamp;
}

DWORD Effect::GetTimeRemaining() const {
    return static_cast<DWORD>(duration * 1000.0f) - GetTimeElapsed();
}

}  // namespace GW::Context
