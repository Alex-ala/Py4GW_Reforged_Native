#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "GW/context/friend_list.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace GW::friend_list {

bool Initialize();
void Shutdown();

using FriendStatusCallback = PY4GW::HookCallback<const Context::Friend*, const Context::Friend*>;
using FriendEventHandlerFn = void(__cdecl*)(void*, void*);
using SetOnlineStatusFn = void(__cdecl*)(Context::FriendStatus status);
using AddFriendFn = void(__cdecl*)(const wchar_t* name, const wchar_t* alias, Context::FriendType type);
using RemoveFriendFn = void(__cdecl*)(const uint8_t* uuid, const wchar_t* name, uint32_t arg8);

Context::FriendList* GetFriendList();

Context::Friend* GetFriend(const wchar_t* alias, const wchar_t* charname, Context::FriendType type = Context::FriendType::Friend);
Context::Friend* GetFriend(uint32_t index);
Context::Friend* GetFriend(const uint8_t* uuid);

uint32_t GetNumberOfFriends(Context::FriendType type = Context::FriendType::Friend);
uint32_t GetNumberOfIgnores();
uint32_t GetNumberOfPartners();
uint32_t GetNumberOfTraders();

Context::FriendStatus GetMyStatus();
bool SetFriendListStatus(Context::FriendStatus status);

void RegisterFriendStatusCallback(
    PY4GW::HookEntry* entry,
    const FriendStatusCallback& callback);
void RemoveFriendStatusCallback(PY4GW::HookEntry* entry);

bool AddFriend(const wchar_t* name, const wchar_t* alias = nullptr);
bool AddIgnore(const wchar_t* name, const wchar_t* alias = nullptr);
bool RemoveFriend(Context::Friend* friend_entry);

extern FriendEventHandlerFn g_friend_event_handler_func;
extern FriendEventHandlerFn g_friend_event_handler_original;
extern SetOnlineStatusFn g_set_online_status_func;
extern AddFriendFn g_add_friend_func;
extern RemoveFriendFn g_remove_friend_func;
extern uintptr_t g_friend_list_addr;
extern std::unordered_map<PY4GW::HookEntry*, FriendStatusCallback> g_friend_status_callbacks;
extern std::atomic<bool> g_initialized;

}  // namespace GW::friend_list
