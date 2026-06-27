#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>

namespace gw::context {

struct WorldContext;

struct GameContext {
    void* h0000;
    void* h0004;
    void* agent;
    void* h000c;
    void* h0010;
    void* map;
    void* text_parser;
    void* h001c;
    uint32_t some_number;
    void* h0024;
    void* account;
    WorldContext* world;
    void* cinematic;
    void* h0034;
    void* gadget;
    void* guild;
    void* items;
    void* character;
    void* h0048;
    void* party;
    void* h0050;
    void* h0054;
    void* trade;
};

static_assert(offsetof(GameContext, world) == 0x2C, "GameContext::world offset mismatch");
static_assert(sizeof(GameContext) == 0x5C, "GameContext size mismatch");

}  // namespace gw::context
