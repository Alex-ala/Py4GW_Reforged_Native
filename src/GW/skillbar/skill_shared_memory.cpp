#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/skill.h"

namespace GW::shared_memory {

void RegisterSkillSegments(Manager& manager) {
    // Global skill data + attribute info: pointer only, materialized in Python.
    manager.SubscribePointer("runtime.ptr.skill_array", [] { return reinterpret_cast<uintptr_t>(Context::GetSkillArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.attribute_info", [] { return reinterpret_cast<uintptr_t>(Context::GetAttributeInfoArray()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
