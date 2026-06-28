#include "base/error_handling.h"

#include "GW/friend_list/friend_list.h"

#include <array>
#include <cwchar>
#include <cstring>

namespace GW::friend_list {

FriendEventHandlerFn g_friend_event_handler_func = nullptr;
FriendEventHandlerFn g_friend_event_handler_original = nullptr;
SetOnlineStatusFn g_set_online_status_func = nullptr;
AddFriendFn g_add_friend_func = nullptr;
RemoveFriendFn g_remove_friend_func = nullptr;
uintptr_t g_friend_list_addr = 0;
std::unordered_map<PY4GW::HookEntry*, FriendStatusCallback> g_friend_status_callbacks;
std::atomic<bool> g_initialized = false;

Context::FriendList* GetFriendList() {
    return reinterpret_cast<Context::FriendList*>(g_friend_list_addr);
}

bool SetFriendListStatus(Context::FriendStatus status) {
    if (!g_set_online_status_func) {
        return false;
    }
    g_set_online_status_func(status);
    return true;
}

void RegisterFriendStatusCallback(
    PY4GW::HookEntry* entry,
    const FriendStatusCallback& callback) {
    RemoveFriendStatusCallback(entry);
    g_friend_status_callbacks[entry] = callback;
}

void RemoveFriendStatusCallback(PY4GW::HookEntry* entry) {
    const auto found = g_friend_status_callbacks.find(entry);
    if (found != g_friend_status_callbacks.end()) {
        g_friend_status_callbacks.erase(found);
    }
}

Context::Friend* GetFriend(const wchar_t* alias, const wchar_t* charname, Context::FriendType type) {
    if (!(alias || charname)) {
        return nullptr;
    }

    const auto* friend_list = GetFriendList();
    if (!friend_list) {
        return nullptr;
    }

    auto& friends = friend_list->friends;
    for (auto friend_entry : friends) {
        if (!(friend_entry && (type == Context::FriendType::Unknow || friend_entry->type == type))) {
            continue;
        }
        if (alias && std::wcsncmp(friend_entry->alias, alias, std::size(friend_entry->alias)) == 0) {
            return friend_entry;
        }
        if (charname && std::wcsncmp(friend_entry->charname, charname, std::size(friend_entry->charname)) == 0) {
            return friend_entry;
        }
    }
    return nullptr;
}

Context::Friend* GetFriend(uint32_t index) {
    const auto* friend_list = GetFriendList();
    if (!friend_list || index >= friend_list->friends.size()) {
        return nullptr;
    }
    return friend_list->friends[index];
}

Context::Friend* GetFriend(const uint8_t* uuid) {
    const auto* friend_list = GetFriendList();
    if (!friend_list) {
        return nullptr;
    }

    auto& friends = friend_list->friends;
    for (auto friend_entry : friends) {
        if (friend_entry && std::memcmp(friend_entry->uuid, uuid, 16) == 0) {
            return friend_entry;
        }
    }
    return nullptr;
}

uint32_t GetNumberOfFriends(Context::FriendType type) {
    const auto* friend_list = GetFriendList();
    if (!friend_list) {
        return 0;
    }

    switch (type) {
    case Context::FriendType::Friend:
        return friend_list->number_of_friend;
    case Context::FriendType::Ignore:
        return friend_list->number_of_ignore;
    case Context::FriendType::Player:
        return friend_list->number_of_partner;
    case Context::FriendType::Trade:
        return friend_list->number_of_trade;
    default:
        return 0;
    }
}

uint32_t GetNumberOfIgnores() {
    return GetNumberOfFriends(Context::FriendType::Ignore);
}

uint32_t GetNumberOfPartners() {
    return GetNumberOfFriends(Context::FriendType::Player);
}

uint32_t GetNumberOfTraders() {
    return GetNumberOfFriends(Context::FriendType::Trade);
}

Context::FriendStatus GetMyStatus() {
    const auto* friend_list = GetFriendList();
    return friend_list ? friend_list->player_status : Context::FriendStatus::Offline;
}

static bool InternalAddFriend(Context::FriendType type, const wchar_t* name, const wchar_t* alias) {
    if (!(g_add_friend_func && name && name[0])) {
        return false;
    }

    wchar_t* buffer = nullptr;
    if (!alias) {
        const size_t length = std::wcslen(name);
        buffer = new wchar_t[length + 1];
        PY4GW_ASSERT(buffer);
        std::wcscpy(buffer, name);
        alias = buffer;
    }

    g_add_friend_func(name, alias, type);
    delete[] buffer;
    return true;
}

bool AddFriend(const wchar_t* name, const wchar_t* alias) {
    return InternalAddFriend(Context::FriendType::Friend, name, alias);
}

bool AddIgnore(const wchar_t* name, const wchar_t* alias) {
    return InternalAddFriend(Context::FriendType::Ignore, name, alias);
}

bool RemoveFriend(Context::Friend* friend_entry) {
    if (!(friend_entry && g_remove_friend_func)) {
        return false;
    }

    g_remove_friend_func(friend_entry->uuid, friend_entry->alias, 0);
    return true;
}

}  // namespace GW::friend_list
