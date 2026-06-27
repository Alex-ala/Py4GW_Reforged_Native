#include "base/error_handling.h"

#include "GW/game_thread/game_thread.h"
#include "GW/game_thread/game_thread_methods.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

void CallFunctions() {
    if (!gw::game_thread::g_initialized) {
        return;
    }

    ::EnterCriticalSection(&gw::game_thread::g_mutex);
    gw::game_thread::g_in_game_thread = true;

    if (!gw::game_thread::g_singleshot_callbacks.empty()) {
        for (const auto& callback : gw::game_thread::g_singleshot_callbacks) {
            callback();
        }
        gw::game_thread::g_singleshot_callbacks.clear();
    }

    py4gw::HookStatus status = {};
    for (auto& entry : gw::game_thread::g_callbacks) {
        entry.callback(&status);
        ++status.altitude;
    }

    gw::game_thread::g_in_game_thread = false;
    ::LeaveCriticalSection(&gw::game_thread::g_mutex);
}

void __cdecl OnLeaveGameThread(void* unk) {
    py4gw::HookBase::EnterHook();
    CallFunctions();
    gw::game_thread::g_leave_game_thread_original(unk);
    py4gw::HookBase::LeaveHook();
}

bool ResolveLeaveGameThreadTarget() {
    CrashContextScope context("startup", "game_thread", "resolve_leave_game_thread_target");
    const auto* pattern = py4gw::Patterns::Get("game_thread.leave_game_thread_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: game_thread.leave_game_thread_target", "game_thread");
        return false;
    }

    const uintptr_t address = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    gw::game_thread::g_leave_game_thread_func = reinterpret_cast<gw::game_thread::LeaveGameThreadFn>(address);
    return Logger::AssertAddress(
        "LeaveGameThread_Func",
        reinterpret_cast<uintptr_t>(gw::game_thread::g_leave_game_thread_func),
        "game_thread");
}

void EnableHooks() {
    CrashContextScope context("runtime", "game_thread", "enable_hooks");
    if (!gw::game_thread::g_initialized || !gw::game_thread::g_leave_game_thread_func) {
        return;
    }

    ::EnterCriticalSection(&gw::game_thread::g_mutex);
    py4gw::HookBase::EnableHooks(reinterpret_cast<void*>(gw::game_thread::g_leave_game_thread_func));
    ::LeaveCriticalSection(&gw::game_thread::g_mutex);
}

void DisableHooks() {
    CrashContextScope context("shutdown", "game_thread", "disable_hooks");
    if (!gw::game_thread::g_initialized || !gw::game_thread::g_leave_game_thread_func) {
        return;
    }

    ::EnterCriticalSection(&gw::game_thread::g_mutex);
    py4gw::HookBase::DisableHooks(reinterpret_cast<void*>(gw::game_thread::g_leave_game_thread_func));
    ::LeaveCriticalSection(&gw::game_thread::g_mutex);
}

void Exit() {
    CrashContextScope context("shutdown", "game_thread", "exit");
    if (!gw::game_thread::g_initialized) {
        return;
    }

    DisableHooks();
    gw::game_thread::ClearCalls();
    py4gw::HookBase::RemoveHook(reinterpret_cast<void*>(gw::game_thread::g_leave_game_thread_func));

    if (gw::game_thread::g_mutex_initialized) {
        ::DeleteCriticalSection(&gw::game_thread::g_mutex);
        gw::game_thread::g_mutex_initialized = false;
    }

    gw::game_thread::g_leave_game_thread_func = nullptr;
    gw::game_thread::g_leave_game_thread_original = nullptr;
    gw::game_thread::g_in_game_thread = false;
}

}  // namespace

namespace gw::game_thread {

bool Initialize() {
    CrashContextScope context("startup", "game_thread", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(py4gw::Scanner::Initialize());
    PY4GW_ASSERT(py4gw::Patterns::Initialize());

    ::InitializeCriticalSection(&g_mutex);
    g_mutex_initialized = true;

    if (!ResolveLeaveGameThreadTarget()) {
        Exit();
        return false;
    }

    py4gw::HookBase::Initialize();
    const int status = py4gw::HookBase::CreateHook(
        reinterpret_cast<void**>(&g_leave_game_thread_func),
        reinterpret_cast<void*>(&OnLeaveGameThread),
        reinterpret_cast<void**>(&g_leave_game_thread_original));
    if (!Logger::AssertHook("LeaveGameThread_Func", status, "game_thread")) {
        Exit();
        py4gw::HookBase::Deinitialize();
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
    py4gw::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace gw::game_thread
