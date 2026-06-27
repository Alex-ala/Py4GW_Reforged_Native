#include "base/error_handling.h"

#include "GW/context/context.h"
#include "GW/context/game_context.h"
#include "GW/context/world_context.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

uintptr_t g_base_ptr = 0;
bool g_initialized = false;

bool ResolveBasePointer() {
    CrashContextScope context("startup", "context", "resolve_base_ptr");
    const auto* pattern = py4gw::Patterns::Get("context.base_ptr_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: context.base_ptr_ref", "context");
        return false;
    }

    const uintptr_t scan = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("base_ptr_ref", scan, "context")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(scan);
    if (!Logger::AssertAddress("base_ptr", candidate, "context")) {
        return false;
    }
    if (!py4gw::Scanner::IsValidPtr(candidate, py4gw::ScannerSection::Data)) {
        Logger::Instance().LogError("base_ptr is outside the expected data section.", "context");
        return false;
    }

    g_base_ptr = candidate;
    return true;
}

}  // namespace

namespace gw::context {

bool Initialize() {
    CrashContextScope context("startup", "context", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(py4gw::Scanner::Initialize());
    PY4GW_ASSERT(py4gw::Patterns::Initialize());

    if (!ResolveBasePointer()) {
        g_base_ptr = 0;
        return false;
    }

    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "context", "shutdown");
    g_base_ptr = 0;
    g_initialized = false;
}

GameContext* GetGameContext() {
    auto** base_context = g_base_ptr ? *reinterpret_cast<uintptr_t***>(g_base_ptr) : nullptr;
    return base_context ? reinterpret_cast<GameContext*>(base_context[0x6]) : nullptr;
}

WorldContext* GetWorldContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->world : nullptr;
}

uint32_t GetControlledCharacterId() {
    WorldContext* world = GetWorldContext();
    return world && world->player_controlled_character ? world->player_controlled_character->agent_id : 0;
}

}  // namespace gw::context
