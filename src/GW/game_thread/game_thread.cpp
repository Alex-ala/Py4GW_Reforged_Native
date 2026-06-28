#include "base/error_handling.h"

#include "GW/game_thread/game_thread.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

void CallFunctions() {
    if (!GW::game_thread::g_initialized) {
        return;
    }

    ::EnterCriticalSection(&GW::game_thread::g_mutex);
    GW::game_thread::g_in_game_thread = true;

    if (!GW::game_thread::g_singleshot_callbacks.empty()) {
        for (const auto& callback : GW::game_thread::g_singleshot_callbacks) {
            callback();
        }
        GW::game_thread::g_singleshot_callbacks.clear();
    }

    PY4GW::HookStatus status = {};
    for (auto& entry : GW::game_thread::g_callbacks) {
        entry.callback(&status);
        ++status.altitude;
    }

    GW::game_thread::g_in_game_thread = false;
    ::LeaveCriticalSection(&GW::game_thread::g_mutex);
}

void __cdecl OnLeaveGameThread(void* unk) {
    PY4GW::HookBase::EnterHook();
    CallFunctions();
    GW::game_thread::g_leave_game_thread_original(unk);
    PY4GW::HookBase::LeaveHook();
}

bool ResolveLeaveGameThreadTarget() {
    CrashContextScope context("startup", "game_thread", "resolve_leave_game_thread_target");
    const auto* pattern = PY4GW::Patterns::Get("game_thread.leave_game_thread_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: game_thread.leave_game_thread_target", "game_thread");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    GW::game_thread::g_leave_game_thread_func = reinterpret_cast<GW::game_thread::LeaveGameThreadFn>(address);
    return Logger::AssertAddress(
        "LeaveGameThread_Func",
        reinterpret_cast<uintptr_t>(GW::game_thread::g_leave_game_thread_func),
        "game_thread");
}

void EnableHooks() {
    CrashContextScope context("runtime", "game_thread", "enable_hooks");
    if (!GW::game_thread::g_initialized || !GW::game_thread::g_leave_game_thread_func) {
        return;
    }

    ::EnterCriticalSection(&GW::game_thread::g_mutex);
    PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(GW::game_thread::g_leave_game_thread_func));
    ::LeaveCriticalSection(&GW::game_thread::g_mutex);
}

void DisableHooks() {
    CrashContextScope context("shutdown", "game_thread", "disable_hooks");
    if (!GW::game_thread::g_initialized || !GW::game_thread::g_leave_game_thread_func) {
        return;
    }

    ::EnterCriticalSection(&GW::game_thread::g_mutex);
    PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(GW::game_thread::g_leave_game_thread_func));
    ::LeaveCriticalSection(&GW::game_thread::g_mutex);
}

void Exit() {
    CrashContextScope context("shutdown", "game_thread", "exit");
    if (!GW::game_thread::g_initialized) {
        return;
    }

    DisableHooks();
    GW::game_thread::ClearCalls();
    PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(GW::game_thread::g_leave_game_thread_func));

    if (GW::game_thread::g_mutex_initialized) {
        ::DeleteCriticalSection(&GW::game_thread::g_mutex);
        GW::game_thread::g_mutex_initialized = false;
    }

    GW::game_thread::g_leave_game_thread_func = nullptr;
    GW::game_thread::g_leave_game_thread_original = nullptr;
    GW::game_thread::g_in_game_thread = false;
}

}  // namespace

namespace GW::game_thread {

bool Initialize() {
    CrashContextScope context("startup", "game_thread", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    ::InitializeCriticalSection(&g_mutex);
    g_mutex_initialized = true;

    if (!ResolveLeaveGameThreadTarget()) {
        Exit();
        return false;
    }

    PY4GW::HookBase::Initialize();
    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_leave_game_thread_func),
        reinterpret_cast<void*>(&OnLeaveGameThread),
        reinterpret_cast<void**>(&g_leave_game_thread_original));
    if (!Logger::AssertHook("LeaveGameThread_Func", status, "game_thread")) {
        Exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    g_initialized = true;
    EnableHooks();
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "game_thread", "shutdown");
    if (!g_initialized) {
        return;
    }

    Exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::game_thread
