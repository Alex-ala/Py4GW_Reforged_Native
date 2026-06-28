#include "base/error_handling.h"

#include "GW/stoc/stoc.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/game_thread/game_thread.h"

#include <string>

void SafeInitializeCriticalSection(CRITICAL_SECTION* mtx) {
    if (!mtx || mtx->DebugInfo) {
        return;
    }
    ::InitializeCriticalSection(&GW::StoC::g_mutex);
    GW::StoC::g_mutex_initialized = true;
}

bool ResolveGameServerHandlers() {
    CrashContextScope context("startup", "stoc", "resolve_game_server_handlers");
    const auto* pattern = PY4GW::Patterns::Get("stoc.handler_table_pointer");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: stoc.handler_table_pointer", "stoc");
        return false;
    }

    const uintptr_t pointer_location = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("StoCHandler_PointerLocation", pointer_location, "stoc")) {
        return false;
    }

    const uintptr_t handlers_addr = *reinterpret_cast<uintptr_t*>(pointer_location);
    if (!Logger::AssertAddress("StoCHandler_Addr", handlers_addr, "stoc")) {
        return false;
    }

    auto** game_server = reinterpret_cast<GW::StoC::GameServer**>(handlers_addr);
    if (!(game_server && *game_server && (*game_server)->gs_codec)) {
        Logger::Instance().LogError("Game server handler table is not fully initialized.", "stoc");
        return false;
    }

    GW::StoC::g_game_server_handlers = &(*game_server)->gs_codec->handlers;
    return GW::StoC::g_game_server_handlers != nullptr;
}

bool __cdecl StoCHandler_Func(GW::Packet::StoC::PacketBase* packet) {
    PY4GW::HookBase::EnterHook();
    PY4GW::HookStatus status = {};

    auto it = GW::StoC::g_packet_entries[packet->header].begin();
    const auto end = GW::StoC::g_packet_entries[packet->header].end();
    while (it != end) {
        if (it->altitude > 0) {
            break;
        }
        it->callback(&status, packet);
        ++status.altitude;
        ++it;
    }

    if (!status.blocked && GW::StoC::g_original_functions) {
        GW::StoC::g_original_functions[packet->header].handler_func(packet);
    }

    while (it != end) {
        it->callback(&status, packet);
        ++status.altitude;
        ++it;
    }

    PY4GW::HookBase::LeaveHook();
    return true;
}

bool OriginalHandler(GW::Packet::StoC::PacketBase* packet) {
    bool ok = false;
    SafeInitializeCriticalSection(&GW::StoC::g_mutex);
    ::EnterCriticalSection(&GW::StoC::g_mutex);
    if (GW::StoC::g_game_server_handlers &&
        GW::StoC::g_original_functions &&
        GW::StoC::g_stoc_handler_count > packet->header) {
        GW::StoC::g_original_functions[packet->header].handler_func(packet);
    }
    ::LeaveCriticalSection(&GW::StoC::g_mutex);
    return ok;
}

void EnableHooks() {
    ::EnterCriticalSection(&GW::StoC::g_mutex);
    GW::StoC::g_hooks_enabled = true;
    for (uint32_t i = 0; GW::StoC::g_original_functions && i < GW::StoC::g_stoc_handler_count; ++i) {
        GW::StoC::g_original_functions[i] = GW::StoC::g_game_server_handlers->at(i);
        if (!GW::StoC::g_packet_entries[i].empty()) {
            GW::StoC::g_game_server_handlers->at(i).handler_func = &StoCHandler_Func;
        }
    }
    ::LeaveCriticalSection(&GW::StoC::g_mutex);
}

void DisableHooks() {
    CrashContextScope context("shutdown", "stoc", "disable_hooks");
    ::EnterCriticalSection(&GW::StoC::g_mutex);
    GW::StoC::g_hooks_enabled = false;
    if (GW::StoC::g_original_functions) {
        for (uint32_t i = 0; GW::StoC::g_game_server_handlers && i < GW::StoC::g_game_server_handlers->size(); ++i) {
            GW::StoC::g_game_server_handlers->at(i).handler_func = GW::StoC::g_original_functions[i].handler_func;
        }
    }
    ::LeaveCriticalSection(&GW::StoC::g_mutex);
}

void InitOnGameThread() {
    CrashContextScope context("startup", "stoc", "init_on_game_thread");
    SafeInitializeCriticalSection(&GW::StoC::g_mutex);
    ::EnterCriticalSection(&GW::StoC::g_mutex);

    if (!ResolveGameServerHandlers() || !GW::StoC::g_game_server_handlers) {
        ::LeaveCriticalSection(&GW::StoC::g_mutex);
        return;
    }

    GW::StoC::g_stoc_handler_count = GW::StoC::g_game_server_handlers->size();
    Logger::Instance().LogInfo("STOC_HEADER_COUNT [" + std::to_string(GW::StoC::g_stoc_handler_count) + "]");
    PY4GW_ASSERT(GW::StoC::g_stoc_handler_count == GW::StoC::kStoCHeaderCount);

    if (!GW::StoC::g_original_functions) {
        GW::StoC::g_original_functions = new GW::StoC::StoCHandler[GW::StoC::g_stoc_handler_count];
        GW::StoC::g_mutex_initialized = true;
    }
    GW::StoC::g_packet_entries.resize(GW::StoC::g_stoc_handler_count);

    EnableHooks();
    GW::StoC::g_initialized = true;
    ::LeaveCriticalSection(&GW::StoC::g_mutex);
}

void Exit() {
    CrashContextScope context("shutdown", "stoc", "exit");
    DisableHooks();

    delete[] GW::StoC::g_original_functions;
    GW::StoC::g_original_functions = nullptr;
    GW::StoC::g_game_server_handlers = nullptr;
    GW::StoC::g_stoc_handler_count = 0;
    GW::StoC::g_packet_entries.clear();

    if (GW::StoC::g_mutex_initialized) {
        ::DeleteCriticalSection(&GW::StoC::g_mutex);
        GW::StoC::g_mutex_initialized = false;
    }
}

namespace GW::StoC {

CRITICAL_SECTION g_mutex;
bool g_mutex_initialized = false;
bool g_hooks_enabled = false;
std::atomic<bool> g_initialized = false;
size_t g_stoc_handler_count = 0;
StoCHandlerArray* g_game_server_handlers = nullptr;
StoCHandler* g_original_functions = nullptr;
std::vector<std::vector<CallbackEntry>> g_packet_entries;

bool Initialize() {
    CrashContextScope context("startup", "stoc", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    SafeInitializeCriticalSection(&g_mutex);
    game_thread::Enqueue([] {
        InitOnGameThread();
    });
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "stoc", "shutdown");
    if (!g_mutex_initialized) {
        return;
    }

    Exit();
    g_initialized = false;
}

}  // namespace GW::stoc
