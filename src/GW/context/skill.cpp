#include "base/error_handling.h"

#include "GW/context/skill.h"

#include "base/memory_manager.h"

#include <iterator>

namespace gw::context {

uint32_t SkillbarSkill::GetRecharge() const {
    if (recharge == 0) {
        return 0;
    }
    return recharge - py4gw::MemoryManager::GetSkillTimer();
}

SkillbarSkill* Skillbar::GetSkillById(gw::constants::SkillID query_skill_id, size_t* slot_out) {
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
    return py4gw::MemoryManager::GetSkillTimer() - timestamp;
}

DWORD Effect::GetTimeRemaining() const {
    return static_cast<DWORD>(duration * 1000.0f) - GetTimeElapsed();
}

}  // namespace gw::context
