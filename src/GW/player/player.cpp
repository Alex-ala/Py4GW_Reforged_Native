#include "base/error_handling.h"

#include "GW/player/player.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <atomic>

namespace GW::player {

using RemoveActiveTitleFn = void(__cdecl*)();
using SetActiveTitleFn = void(__cdecl*)(uint32_t identifier);
using DepositFactionFn = void(__cdecl*)(uint32_t always_0, uint32_t allegiance, uint32_t amount);

bool ResolveSetActiveTitle();
bool ResolveRemoveActiveTitle();
bool ResolveDepositFaction();
bool ResolveTitleData();
bool ResolveAvailableChars();

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

RemoveActiveTitleFn g_remove_active_title_func = nullptr;
SetActiveTitleFn g_set_active_title_func = nullptr;
DepositFactionFn g_deposit_faction_func = nullptr;
GWArray<Context::AvailableCharacterInfo>* g_available_chars = nullptr;
std::atomic<bool> g_initialized = false;

bool Init() {
    CrashContextScope context("startup", "player", "init");
    if (!(ResolveSetActiveTitle() &&
          ResolveRemoveActiveTitle() &&
          ResolveDepositFaction() &&
          ResolveTitleData())) {
        return false;
    }
    // Non-fatal: the account-roster global is also resolved lazily on first use
    // (parity with legacy AccountMgr::GetAvailableChars). A miss must not abort
    // the whole player module / GW init.
    ResolveAvailableChars();
    return true;
}

void EnableHooks() {
}

void DisableHooks() {
}

void Exit() {
    CrashContextScope context("shutdown", "player", "exit");
    g_remove_active_title_func = nullptr;
    g_set_active_title_func = nullptr;
    g_deposit_faction_func = nullptr;
    g_available_chars = nullptr;
}

bool Initialize() {
    CrashContextScope context("startup", "player", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    if (!Init()) {
        Exit();
        return false;
    }

    EnableHooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "player", "shutdown");
    if (!g_initialized) {
        return;
    }

    DisableHooks();
    Exit();
    g_initialized = false;
}

}  // namespace GW::player
