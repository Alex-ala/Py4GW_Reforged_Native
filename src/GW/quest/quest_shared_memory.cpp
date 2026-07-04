#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/quest.h"

namespace GW::shared_memory {

void RegisterQuestSegments(Manager& manager) {
    manager.SubscribePointer("runtime.ptr.quest_log", [] { return reinterpret_cast<uintptr_t>(Context::GetQuestLog()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
