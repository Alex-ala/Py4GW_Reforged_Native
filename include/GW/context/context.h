#pragma once

#include "base/error_handling.h"

#include <cstdint>

namespace gw::context {

struct GameContext;
struct WorldContext;

bool Initialize();
void Shutdown();

GameContext* GetGameContext();
WorldContext* GetWorldContext();
uint32_t GetControlledCharacterId();

}  // namespace gw::context
