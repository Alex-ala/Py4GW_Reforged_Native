#pragma once

#include "base/error_handling.h"

#include "GW/shared_memory/manager.h"

// Per-module shared-memory subscription registrars. Each is implemented in its
// module's <module>_shared_memory.cpp, one commentable line per struct. The
// aggregator RegisterDefaultSegments() calls them in order.
namespace GW::shared_memory {

void RegisterContextSegments(Manager& manager);   // pointers directory + char/world/game/gameplay/account/pregame/text/salvage
void RegisterAgentSegments(Manager& manager);     // agent context + classified agent array
void RegisterMapSegments(Manager& manager);       // map/mission-map/world-map + map summary
void RegisterPartySegments(Manager& manager);
void RegisterItemSegments(Manager& manager);
void RegisterGuildSegments(Manager& manager);
void RegisterTradeSegments(Manager& manager);
void RegisterFriendListSegments(Manager& manager);
void RegisterCameraSegments(Manager& manager);
void RegisterRenderSegments(Manager& manager);
void RegisterSkillSegments(Manager& manager);       // global skill data + attribute info (pointers)
void RegisterQuestSegments(Manager& manager);       // quest log (pointer)

}  // namespace GW::shared_memory
