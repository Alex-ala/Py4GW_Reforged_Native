#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"

#include <atomic>
#include <functional>
#include <vector>
#include <windows.h>

namespace gw::game_thread {

using GameThreadCallback = py4gw::HookCallback<>;

void ClearCalls();
void Enqueue(std::function<void()> callback);
void RegisterGameThreadCallback(
    py4gw::HookEntry* entry,
    const GameThreadCallback& callback,
    int altitude = 0x4000);
void RemoveGameThreadCallback(py4gw::HookEntry* entry);
bool IsInGameThread();

using LeaveGameThreadFn = void(__cdecl*)(void*);

struct CallbackEntry {
    int altitude = 0;
    py4gw::HookEntry* entry = nullptr;
    GameThreadCallback callback;
};

extern CRITICAL_SECTION g_mutex;
extern bool g_mutex_initialized;
extern LeaveGameThreadFn g_leave_game_thread_func;
extern LeaveGameThreadFn g_leave_game_thread_original;
extern std::atomic<bool> g_initialized;
extern std::atomic<bool> g_in_game_thread;
extern std::vector<std::function<void()>> g_singleshot_callbacks;
extern std::vector<CallbackEntry> g_callbacks;

}  // namespace gw::game_thread
