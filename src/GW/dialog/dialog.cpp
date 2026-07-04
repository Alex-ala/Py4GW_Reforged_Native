#include "base/error_handling.h"

#include "GW/dialog/dialog.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "GW/agent/agent.h"
#include "GW/context/agent.h"
#include "GW/context/ui.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <new>
#include <unordered_map>

// Merged port of legacy py_dialog.cpp (class Dialog) and py_dialog_catalog.cpp
// (class DialogCatalog). Runtime dialog state is guarded by dialog_mutex and
// the decoded-text catalog cache by catalog_mutex - the two-lock split is
// load-bearing: the send-dialog UI handler queries the catalog cache while
// holding dialog_mutex (see OnDialogUIMessage), so merging the locks would
// self-deadlock.
namespace GW::dialog {

namespace {

using UIMessage = GW::ui::UIMessage;

constexpr auto kDialogAsyncDrainTimeout = std::chrono::milliseconds(500);
constexpr auto kDialogCallbackResumeDelay = std::chrono::milliseconds(100);

constexpr size_t kMaxDialogEventLogs = 512;
constexpr size_t kMaxDialogCallbackJournal = 1000;
constexpr size_t kMaxActiveDialogButtons = 64;
constexpr size_t kMaxDecodedButtonLabelCache = 256;
constexpr size_t kMaxDecodedButtonLabelPending = 128;

struct DialogMapStateSnapshot {
    uint32_t map_id = 0;
    bool map_ready = false;
};

struct DialogBodyDecodeRequest {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    uint32_t agent_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t map_id = 0;
    uint32_t model_id = 0;
    uint64_t decode_epoch = 0;
    uint64_t decode_nonce = 0;
    wchar_t* encoded = nullptr;
};

struct DialogButtonDecodeRequest {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
    uint32_t map_id = 0;
    uint32_t model_id = 0;
    uint64_t decode_epoch = 0;
    wchar_t* encoded = nullptr;
};

struct DialogDecodeRequest {
    uint32_t dialog_id = 0;
    uint64_t decode_epoch = 0;
    wchar_t* encoded = nullptr;
};

/* ---------------- Runtime dialog state (legacy Dialog statics) ---------------- */

std::mutex dialog_mutex;
ActiveDialogInfo active_dialog_cache = {0, 0, 0, false, L""};
std::vector<DialogButtonInfo> active_dialog_buttons;
std::unordered_map<uint32_t, std::string> decoded_button_label_cache;
std::unordered_map<uint32_t, bool> decoded_button_label_pending;
bool dialog_hook_registered = false;
PY4GW::HookEntry dialog_ui_message_entry_body;
PY4GW::HookEntry dialog_ui_message_entry_button;
PY4GW::HookEntry dialog_ui_message_entry_send_agent;
PY4GW::HookEntry dialog_ui_message_entry_send_gadget;
uint32_t last_selected_dialog_id = 0;
uint32_t pending_body_context_dialog_id = 0;
uint32_t pending_body_context_agent_id = 0;
std::vector<DialogEventLog> dialog_event_logs;
std::vector<DialogEventLog> dialog_event_logs_received;
std::vector<DialogEventLog> dialog_event_logs_sent;
std::vector<DialogCallbackJournalEntry> dialog_callback_journal;
std::vector<DialogCallbackJournalEntry> dialog_callback_journal_received;
std::vector<DialogCallbackJournalEntry> dialog_callback_journal_sent;
std::condition_variable dialog_async_decode_drained;
uint32_t dialog_pending_async_decode_count = 0;
uint64_t dialog_decode_epoch = 0;
bool dialog_shutdown_requested = false;
bool dialog_callbacks_suspended = true;
uint64_t active_dialog_body_decode_nonce = 0;
uint32_t last_observed_map_id = 0;
bool last_observed_map_ready = false;
uint64_t dialog_callbacks_resume_tick = 0;

/* ------------- Catalog decode state (legacy DialogCatalog statics) ------------- */

std::mutex catalog_mutex;
std::unordered_map<uint32_t, std::string> decoded_text_cache;
std::unordered_map<uint32_t, bool> decoded_text_pending;
std::condition_variable catalog_async_decode_drained;
uint32_t catalog_pending_async_decode_count = 0;
uint64_t catalog_decode_epoch = 0;
bool catalog_shutdown_requested = false;

/* ---------------- SEH-guarded helpers ---------------- */

std::string WideToUtf8Safe(const wchar_t* wstr) {
    if (!wstr) {
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }
    try {
        std::string out(static_cast<size_t>(len), '\0');
        const int written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
        if (written <= 0) {
            return {};
        }
        out.resize(static_cast<size_t>(written - 1));
        return out;
    } catch (...) {
        return {};
    }
}

std::wstring Utf8ToWideSafe(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    try {
        std::wstring out(static_cast<size_t>(len), L'\0');
        const int written = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
        if (written <= 0) {
            return {};
        }
        out.resize(static_cast<size_t>(written - 1));
        return out;
    } catch (...) {
        return {};
    }
}

DialogMapStateSnapshot GetDialogMapStateSafe() {
    DialogMapStateSnapshot snapshot{};
    __try {
        snapshot.map_id = static_cast<uint32_t>(GW::map::GetMapID());
        const auto instance_type = GW::map::GetInstanceType();
        snapshot.map_ready = GW::map::GetIsMapLoaded() &&
            !GW::map::GetIsObserving() &&
            instance_type != GW::Constants::InstanceType::Loading;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot.map_id = 0;
        snapshot.map_ready = false;
    }
    return snapshot;
}

bool IsDialogMapReadySafe() {
    __try {
        const auto instance_type = GW::map::GetInstanceType();
        return GW::map::GetIsMapLoaded() &&
            !GW::map::GetIsObserving() &&
            instance_type != GW::Constants::InstanceType::Loading;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint32_t GetCurrentMapIdSafe() {
    __try {
        return static_cast<uint32_t>(GW::map::GetMapID());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

uint32_t GetAgentModelIdSafe(uint32_t agent_id) {
    if (!agent_id) {
        return 0;
    }
    __try {
        GW::Context::Agent* agent = GW::agent::GetAgentByID(agent_id);
        if (!agent) {
            return 0;
        }
        GW::Context::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living) {
            return 0;
        }
        return static_cast<uint32_t>(living->player_number);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

std::string BuildNpcUid(uint32_t map_id, uint32_t model_id, uint32_t agent_id) {
    if (!agent_id) {
        return {};
    }
    char buffer[96] = {};
    std::snprintf(buffer, sizeof(buffer), "%u:%u:%u", map_id, model_id, agent_id);
    return std::string(buffer);
}

bool SafeIsValidEncStr(const wchar_t* enc_str) {
    __try {
        return GW::ui::IsValidEncStr(enc_str);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

wchar_t* DupWideStringSafe(const wchar_t* src) {
    if (!src) {
        return nullptr;
    }
    __try {
        size_t len = wcslen(src);
        auto* buf = new (std::nothrow) wchar_t[len + 1];
        if (!buf) {
            return nullptr;
        }
        std::wmemcpy(buf, src, len + 1);
        return buf;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* SafeCallDialogLoader_GetText(DialogMemory::DialogLoader_GetText_fn fn, uint32_t dialog_id) {
    void* result = nullptr;
    __try {
        result = fn(dialog_id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = nullptr;
    }
    return result;
}

bool SafeAsyncDecodeStr(const wchar_t* encoded, GW::ui::DecodeStr_Callback callback, void* callback_param) {
    if (!encoded || !callback) {
        return false;
    }
    __try {
        GW::ui::AsyncDecodeStr(encoded, callback, callback_param);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CopyBytesNoFault(const void* src, size_t size, void* dst) {
    __try {
        std::memcpy(dst, src, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void CopyBytesSafe(const void* src, size_t size, std::vector<uint8_t>& out) {
    out.clear();
    if (!src || size == 0) {
        return;
    }
    try {
        out.resize(size);
    } catch (...) {
        out.clear();
        return;
    }
    if (!CopyBytesNoFault(src, size, out.data())) {
        out.clear();
    }
}

bool CopyDialogButtonInfoSafe(const void* src, GW::Context::DialogButtonInfo& out) {
    std::memset(&out, 0, sizeof(out));
    if (!src) {
        return false;
    }
    __try {
        std::memcpy(&out, src, sizeof(out));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(&out, 0, sizeof(out));
        return false;
    }
}

bool CopyDialogBodyInfoSafe(const void* src, GW::Context::DialogBodyInfo& out) {
    std::memset(&out, 0, sizeof(out));
    if (!src) {
        return false;
    }
    __try {
        std::memcpy(&out, src, sizeof(out));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(&out, 0, sizeof(out));
        return false;
    }
}

void LogMemoryReadFailure(const char* label, uintptr_t address) {
    char buffer[256];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[dialog] Failed to read %s at 0x%08X",
        label,
        static_cast<uint32_t>(address));
    Logger::Instance().LogInfo(buffer);
}

bool TryReadU32(uintptr_t address, uint32_t& out, const char* label) {
    __try {
        out = *reinterpret_cast<uint32_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        LogMemoryReadFailure(label, address);
        return false;
    }
}

void ReleaseDialogBodyDecodeRequest(DialogBodyDecodeRequest* req) {
    if (!req) {
        return;
    }
    delete[] req->encoded;
    delete req;
}

void ReleaseDialogButtonDecodeRequest(DialogButtonDecodeRequest* req) {
    if (!req) {
        return;
    }
    delete[] req->encoded;
    delete req;
}

void ReleaseDialogDecodeRequest(DialogDecodeRequest* req) {
    if (!req) {
        return;
    }
    delete[] req->encoded;
    delete req;
}

bool TryUnregisterDialogUiHooksRaw(
    PY4GW::HookEntry* body_entry,
    PY4GW::HookEntry* button_entry,
    PY4GW::HookEntry* send_agent_entry,
    PY4GW::HookEntry* send_gadget_entry) {
    __try {
        GW::ui::RemoveUIMessageCallback(body_entry, UIMessage::kDialogBody);
        GW::ui::RemoveUIMessageCallback(button_entry, UIMessage::kDialogButton);
        GW::ui::RemoveUIMessageCallback(send_agent_entry, UIMessage::kSendAgentDialog);
        GW::ui::RemoveUIMessageCallback(send_gadget_entry, UIMessage::kSendGadgetDialog);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int DialogCallbackJournalEventPriority(const std::string& event_type) {
    if (event_type == "recv_body") {
        return 0;
    }
    if (event_type == "recv_choice") {
        return 1;
    }
    if (event_type == "sent_choice") {
        return 2;
    }
    return 3;
}

bool DialogCallbackJournalChronologicalLess(
    const DialogCallbackJournalEntry& lhs,
    const DialogCallbackJournalEntry& rhs) {
    if (lhs.tick != rhs.tick) {
        return lhs.tick < rhs.tick;
    }

    const int lhs_priority = DialogCallbackJournalEventPriority(lhs.event_type);
    const int rhs_priority = DialogCallbackJournalEventPriority(rhs.event_type);
    if (lhs_priority != rhs_priority) {
        return lhs_priority < rhs_priority;
    }

    if (lhs.incoming != rhs.incoming) {
        return lhs.incoming && !rhs.incoming;
    }

    return false;
}

std::vector<DialogCallbackJournalEntry> SortDialogCallbackJournalEntries(
    std::vector<DialogCallbackJournalEntry> entries) {
    std::stable_sort(
        entries.begin(),
        entries.end(),
        DialogCallbackJournalChronologicalLess);
    return entries;
}

/* ---------------- Internal forward declarations ---------------- */

void ObserveMapChange(uint32_t current_map_id, bool current_map_ready, bool log_transition);
void AppendDialogCallbackJournalEntry(
    uint64_t tick,
    uint32_t message_id,
    bool incoming,
    const char* event_type,
    uint32_t dialog_id,
    uint32_t context_dialog_id,
    uint32_t agent_id,
    bool dialog_id_authoritative,
    bool context_dialog_id_inferred,
    std::optional<uint32_t> map_id = std::nullopt,
    std::optional<uint32_t> model_id = std::nullopt,
    const std::string& text = std::string{});
void __cdecl OnDialogBodyDecoded(void* param, const wchar_t* s);
void __cdecl OnDialogButtonDecoded(void* param, const wchar_t* s);
void __cdecl OnDialogTextDecoded(void* param, const wchar_t* s);
void QueueDialogTextDecode(uint32_t dialog_id);
void ClearCatalogCache();

/* ---------------- Event log / journal appenders ---------------- */

void AppendDialogEventLog(
    UIMessage msgid,
    bool incoming,
    bool is_frame_message,
    uint32_t frame_id,
    const void* wparam,
    size_t wparam_size,
    const void* lparam,
    size_t lparam_size
) {
    DialogEventLog entry{};
    entry.tick = GetTickCount64();
    entry.message_id = static_cast<uint32_t>(msgid);
    entry.incoming = incoming;
    entry.is_frame_message = is_frame_message;
    entry.frame_id = frame_id;
    CopyBytesSafe(wparam, wparam_size, entry.w_bytes);
    CopyBytesSafe(lparam, lparam_size, entry.l_bytes);
    std::scoped_lock lock(dialog_mutex);
    try {
        dialog_event_logs.push_back(std::move(entry));
        if (dialog_event_logs.size() > kMaxDialogEventLogs) {
            const size_t overflow = dialog_event_logs.size() - kMaxDialogEventLogs;
            dialog_event_logs.erase(dialog_event_logs.begin(),
                dialog_event_logs.begin() + overflow);
        }
        auto& dir_logs = incoming ? dialog_event_logs_received : dialog_event_logs_sent;
        dir_logs.push_back(dialog_event_logs.back());
        if (dir_logs.size() > kMaxDialogEventLogs) {
            const size_t overflow = dir_logs.size() - kMaxDialogEventLogs;
            dir_logs.erase(dir_logs.begin(), dir_logs.begin() + overflow);
        }
    } catch (...) {
    }
}

void AppendDialogCallbackJournalEntry(
    uint64_t tick,
    uint32_t message_id,
    bool incoming,
    const char* event_type,
    uint32_t dialog_id,
    uint32_t context_dialog_id,
    uint32_t agent_id,
    bool dialog_id_authoritative,
    bool context_dialog_id_inferred,
    std::optional<uint32_t> map_id,
    std::optional<uint32_t> model_id,
    const std::string& text
) {
    DialogCallbackJournalEntry entry{};
    entry.tick = tick ? tick : GetTickCount64();
    entry.message_id = message_id;
    entry.incoming = incoming;
    entry.dialog_id = dialog_id;
    entry.context_dialog_id = context_dialog_id;
    entry.agent_id = agent_id;
    entry.map_id = map_id.has_value() ? *map_id : GetCurrentMapIdSafe();
    entry.model_id = model_id.has_value() ? *model_id : GetAgentModelIdSafe(agent_id);
    entry.dialog_id_authoritative = dialog_id_authoritative;
    entry.context_dialog_id_inferred = context_dialog_id_inferred;
    try {
        entry.npc_uid = BuildNpcUid(entry.map_id, entry.model_id, agent_id);
        entry.event_type = event_type ? event_type : "";
        entry.text = text;
    } catch (...) {
        return;
    }

    std::scoped_lock lock(dialog_mutex);
    try {
        dialog_callback_journal.push_back(entry);
        if (dialog_callback_journal.size() > kMaxDialogCallbackJournal) {
            const size_t overflow = dialog_callback_journal.size() - kMaxDialogCallbackJournal;
            dialog_callback_journal.erase(
                dialog_callback_journal.begin(),
                dialog_callback_journal.begin() + overflow);
        }

        auto& dir_logs = incoming ? dialog_callback_journal_received : dialog_callback_journal_sent;
        dir_logs.push_back(std::move(entry));
        if (dir_logs.size() > kMaxDialogCallbackJournal) {
            const size_t overflow = dir_logs.size() - kMaxDialogCallbackJournal;
            dir_logs.erase(dir_logs.begin(), dir_logs.begin() + overflow);
        }
    } catch (...) {
    }
}

/* ---------------- Map transition tracking ---------------- */

void ObserveMapChange(uint32_t current_map_id, bool current_map_ready, bool /*log_transition*/) {
    const uint64_t now = GetTickCount64();
    {
        std::scoped_lock lock(dialog_mutex);
        const uint32_t previous_map_id = last_observed_map_id;
        const bool previous_map_ready = last_observed_map_ready;
        const bool map_id_changed = previous_map_id != current_map_id;
        const bool map_ready_changed = previous_map_ready != current_map_ready;
        if (!map_id_changed && !map_ready_changed) {
            return;
        }

        last_observed_map_id = current_map_id;
        last_observed_map_ready = current_map_ready;
        if (dialog_shutdown_requested) {
            return;
        }

        if (!current_map_ready || map_id_changed) {
            dialog_callbacks_suspended = true;
            dialog_callbacks_resume_tick = now + static_cast<uint64_t>(kDialogCallbackResumeDelay.count());
        }

        const bool should_invalidate_runtime_state =
            (map_id_changed && previous_map_id != 0) ||
            (previous_map_ready && !current_map_ready);
        if (!should_invalidate_runtime_state) {
            return;
        }

        active_dialog_cache = {0, 0, 0, false, L""};
        active_dialog_buttons.clear();
        last_selected_dialog_id = 0;
        pending_body_context_dialog_id = 0;
        pending_body_context_agent_id = 0;
        decoded_button_label_cache.clear();
        decoded_button_label_pending.clear();
        ++dialog_decode_epoch;
        ++active_dialog_body_decode_nonce;
    }
}

/* ---------------- UI message handler ---------------- */

void OnDialogUIMessage(PY4GW::HookStatus*, UIMessage message_id, void* wparam, void*) {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    if (!wparam) {
        return;
    }
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_shutdown_requested || dialog_callbacks_suspended || !map_state.map_ready) {
            return;
        }
    }

    switch (message_id) {
        case UIMessage::kDialogButton: {
            GW::Context::DialogButtonInfo info_local{};
            if (!CopyDialogButtonInfoSafe(wparam, info_local)) {
                return;
            }
            const auto* info = &info_local;
            const uint64_t tick = GetTickCount64();
            uint32_t context_dialog_id = 0;
            uint32_t context_agent_id = 0;
            uint64_t request_epoch = 0;
            {
                std::scoped_lock lock(dialog_mutex);
                context_dialog_id = active_dialog_cache.context_dialog_id
                    ? active_dialog_cache.context_dialog_id
                    : active_dialog_cache.dialog_id;
                context_agent_id = active_dialog_cache.agent_id;
                request_epoch = dialog_decode_epoch;
            }
            const uint32_t callback_map_id = map_state.map_id;
            const uint32_t callback_model_id = GetAgentModelIdSafe(context_agent_id);
            AppendDialogEventLog(
                message_id,
                true,
                false,
                0,
                info,
                sizeof(GW::Context::DialogButtonInfo),
                nullptr,
                0
            );
            std::string label_utf8;
            bool label_pending = false;
            if (info->message) {
                wchar_t* encoded_copy = DupWideStringSafe(info->message);
                if (encoded_copy) {
                    if (!SafeIsValidEncStr(encoded_copy)) {
                        label_utf8 = WideToUtf8Safe(encoded_copy);
                        delete[] encoded_copy;
                    }
                    else {
                        auto* req = new (std::nothrow) DialogButtonDecodeRequest();
                        if (!req) {
                            delete[] encoded_copy;
                            break;
                        }
                        bool queue_async_label = false;
                        req->tick = tick;
                        req->message_id = static_cast<uint32_t>(message_id);
                        req->dialog_id = info->dialog_id;
                        req->context_dialog_id = context_dialog_id;
                        req->agent_id = context_agent_id;
                        req->map_id = callback_map_id;
                        req->model_id = callback_model_id;
                        req->decode_epoch = request_epoch;
                        req->encoded = encoded_copy;
                        bool release_req = false;
                        {
                            std::scoped_lock lock(dialog_mutex);
                            if (dialog_shutdown_requested ||
                                dialog_callbacks_suspended ||
                                req->decode_epoch != dialog_decode_epoch) {
                                release_req = true;
                            }
                            else {
                                try {
                                    const bool is_new_pending =
                                        decoded_button_label_pending.find(info->dialog_id) == decoded_button_label_pending.end();
                                    if (is_new_pending &&
                                        decoded_button_label_pending.size() >= kMaxDecodedButtonLabelPending) {
                                        release_req = true;
                                    }
                                    else {
                                        decoded_button_label_pending[info->dialog_id] = true;
                                        ++dialog_pending_async_decode_count;
                                        queue_async_label = true;
                                    }
                                } catch (...) {
                                    decoded_button_label_pending.erase(info->dialog_id);
                                    release_req = true;
                                }
                            }
                        }
                        if (release_req) {
                            ReleaseDialogButtonDecodeRequest(req);
                        }
                        else if (queue_async_label) {
                            if (SafeAsyncDecodeStr(req->encoded, OnDialogButtonDecoded, req)) {
                                label_pending = true;
                            }
                            else {
                                {
                                    std::scoped_lock lock(dialog_mutex);
                                    if (dialog_pending_async_decode_count > 0) {
                                        --dialog_pending_async_decode_count;
                                    }
                                    decoded_button_label_pending.erase(info->dialog_id);
                                }
                                dialog_async_decode_drained.notify_all();
                                ReleaseDialogButtonDecodeRequest(req);
                            }
                        }
                    }
                }
            }

            {
                std::scoped_lock lock(dialog_mutex);
                try {
                    if (request_epoch == dialog_decode_epoch &&
                        !dialog_shutdown_requested &&
                        !dialog_callbacks_suspended &&
                        !label_utf8.empty()) {
                        decoded_button_label_cache[info->dialog_id] = label_utf8;
                        if (decoded_button_label_cache.size() > kMaxDecodedButtonLabelCache) {
                            decoded_button_label_cache.erase(decoded_button_label_cache.begin());
                        }
                        decoded_button_label_pending.erase(info->dialog_id);
                    }
                    if (request_epoch == dialog_decode_epoch &&
                        !dialog_shutdown_requested &&
                        !dialog_callbacks_suspended) {
                        DialogButtonInfo button{};
                        button.dialog_id = info->dialog_id;
                        button.button_icon = info->button_icon;
                        button.message = label_utf8;
                        button.message_decoded = label_utf8;
                        button.message_decode_pending = label_pending;
                        active_dialog_buttons.push_back(std::move(button));
                        if (active_dialog_buttons.size() > kMaxActiveDialogButtons) {
                            const size_t overflow = active_dialog_buttons.size() - kMaxActiveDialogButtons;
                            active_dialog_buttons.erase(
                                active_dialog_buttons.begin(),
                                active_dialog_buttons.begin() + overflow);
                        }
                    }
                } catch (...) {
                    if (request_epoch == dialog_decode_epoch) {
                        decoded_button_label_pending.erase(info->dialog_id);
                    }
                }
            }
            if (!label_pending) {
                bool append_journal = false;
                {
                    std::scoped_lock lock(dialog_mutex);
                    append_journal =
                        request_epoch == dialog_decode_epoch &&
                        !dialog_shutdown_requested &&
                        !dialog_callbacks_suspended;
                }
                if (append_journal) {
                    AppendDialogCallbackJournalEntry(
                        tick,
                        static_cast<uint32_t>(message_id),
                        true,
                        "recv_choice",
                        info->dialog_id,
                        context_dialog_id,
                        context_agent_id,
                        true,
                        context_dialog_id != 0,
                        callback_map_id,
                        callback_model_id,
                        label_utf8
                    );
                }
            }
        } break;
        case UIMessage::kDialogBody: {
            GW::Context::DialogBodyInfo info_local{};
            if (!CopyDialogBodyInfoSafe(wparam, info_local)) {
                return;
            }
            const auto* info = &info_local;
            const uint64_t tick = GetTickCount64();
            const uint32_t callback_map_id = map_state.map_id;

            AppendDialogEventLog(
                message_id,
                true,
                false,
                0,
                info,
                sizeof(GW::Context::DialogBodyInfo),
                nullptr,
                0
            );

            uint32_t context_dialog_id = 0;
            uint64_t request_epoch = 0;
            uint64_t decode_nonce = 0;
            bool body_state_active = false;
            {
                std::scoped_lock lock(dialog_mutex);
                if (dialog_shutdown_requested || dialog_callbacks_suspended) {
                    break;
                }
                active_dialog_cache.agent_id = info->agent_id;
                if (pending_body_context_dialog_id != 0) {
                    const bool same_agent =
                        pending_body_context_agent_id == 0 ||
                        pending_body_context_agent_id == info->agent_id;
                    if (same_agent) {
                        context_dialog_id = pending_body_context_dialog_id;
                    }
                }
                pending_body_context_dialog_id = 0;
                pending_body_context_agent_id = 0;
                active_dialog_cache.dialog_id = 0;
                active_dialog_cache.context_dialog_id = context_dialog_id;
                active_dialog_cache.dialog_id_authoritative = false;
                active_dialog_cache.message.clear();
                active_dialog_buttons.clear();
                request_epoch = dialog_decode_epoch;
                decode_nonce = ++active_dialog_body_decode_nonce;
                body_state_active = true;
            }
            if (!body_state_active) {
                break;
            }

            bool append_immediate = true;
            std::string immediate_text;
            const uint32_t callback_model_id = GetAgentModelIdSafe(info->agent_id);

            if (info->message_enc) {
                wchar_t* encoded_copy = DupWideStringSafe(info->message_enc);
                if (encoded_copy) {
                    if (!SafeIsValidEncStr(encoded_copy)) {
                        std::wstring plain_text;
                        try {
                            plain_text.assign(encoded_copy);
                        } catch (...) {
                        }
                        immediate_text = WideToUtf8Safe(encoded_copy);
                        delete[] encoded_copy;
                        {
                            std::scoped_lock lock(dialog_mutex);
                            if (!dialog_shutdown_requested &&
                                !dialog_callbacks_suspended &&
                                request_epoch == dialog_decode_epoch &&
                                active_dialog_cache.agent_id == info->agent_id &&
                                active_dialog_body_decode_nonce == decode_nonce) {
                                try {
                                    active_dialog_cache.message = plain_text;
                                } catch (...) {
                                }
                            }
                        }
                    }
                    else {
                        auto* req = new (std::nothrow) DialogBodyDecodeRequest();
                        if (!req) {
                            delete[] encoded_copy;
                            break;
                        }
                        bool queue_async_body = false;
                        req->tick = tick;
                        req->message_id = static_cast<uint32_t>(message_id);
                        req->agent_id = info->agent_id;
                        req->context_dialog_id = context_dialog_id;
                        req->map_id = callback_map_id;
                        req->model_id = callback_model_id;
                        req->decode_epoch = request_epoch;
                        req->decode_nonce = decode_nonce;
                        req->encoded = encoded_copy;
                        {
                            std::scoped_lock lock(dialog_mutex);
                            if (dialog_shutdown_requested ||
                                dialog_callbacks_suspended ||
                                req->decode_epoch != dialog_decode_epoch) {
                                delete[] req->encoded;
                                delete req;
                            } else {
                                ++dialog_pending_async_decode_count;
                                queue_async_body = true;
                            }
                        }
                        if (queue_async_body) {
                            if (SafeAsyncDecodeStr(req->encoded, OnDialogBodyDecoded, req)) {
                                append_immediate = false;
                            }
                            else {
                                {
                                    std::scoped_lock lock(dialog_mutex);
                                    if (dialog_pending_async_decode_count > 0) {
                                        --dialog_pending_async_decode_count;
                                    }
                                }
                                dialog_async_decode_drained.notify_all();
                                ReleaseDialogBodyDecodeRequest(req);
                            }
                        }
                    }
                }
            }
            if (append_immediate) {
                bool append_journal = false;
                {
                    std::scoped_lock lock(dialog_mutex);
                    append_journal =
                        request_epoch == dialog_decode_epoch &&
                        !dialog_shutdown_requested &&
                        !dialog_callbacks_suspended;
                }
                if (append_journal) {
                    AppendDialogCallbackJournalEntry(
                        tick,
                        static_cast<uint32_t>(message_id),
                        true,
                        "recv_body",
                        0,
                        context_dialog_id,
                        info->agent_id,
                        false,
                        context_dialog_id != 0,
                        callback_map_id,
                        callback_model_id,
                        immediate_text
                    );
                }
            }
        } break;
        case UIMessage::kSendAgentDialog:
        case UIMessage::kSendGadgetDialog: {
            const uint64_t tick = GetTickCount64();
            uint32_t selected_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            AppendDialogEventLog(
                message_id,
                false,
                false,
                0,
                &selected_id,
                sizeof(selected_id),
                nullptr,
                0
            );
            uint32_t context_dialog_id = 0;
            uint32_t context_agent_id = 0;
            std::string sent_text;
            bool emit_sent_choice = false;
            {
                std::scoped_lock lock(dialog_mutex);
                if (dialog_shutdown_requested || dialog_callbacks_suspended) {
                    break;
                }
                context_dialog_id = active_dialog_cache.context_dialog_id
                    ? active_dialog_cache.context_dialog_id
                    : active_dialog_cache.dialog_id;
                context_agent_id = active_dialog_cache.agent_id;
                auto label_it = decoded_button_label_cache.find(selected_id);
                try {
                    if (label_it != decoded_button_label_cache.end()) {
                        sent_text = label_it->second;
                    }
                    else if (!TryGetCachedDialogTextDecoded(selected_id, sent_text)) {
                        sent_text.clear();
                    }
                } catch (...) {
                    sent_text.clear();
                }
                last_selected_dialog_id = selected_id;
                pending_body_context_dialog_id = selected_id;
                pending_body_context_agent_id = context_agent_id;
                active_dialog_cache.dialog_id = 0;
                active_dialog_cache.context_dialog_id = selected_id;
                active_dialog_cache.dialog_id_authoritative = false;
                emit_sent_choice = true;
            }
            if (emit_sent_choice) {
                AppendDialogCallbackJournalEntry(
                    tick,
                    static_cast<uint32_t>(message_id),
                    false,
                    "sent_choice",
                    selected_id,
                    context_dialog_id,
                    context_agent_id,
                    true,
                    context_dialog_id != 0,
                    std::nullopt,
                    std::nullopt,
                    sent_text
                );
            }
        } break;
        default:
            break;
    }
}

/* ---------------- Async decode completions ---------------- */

void __cdecl OnDialogBodyDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogBodyDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    wchar_t* decoded_copy = DupWideStringSafe(s);
    std::wstring decoded_w;
    if (decoded_copy) {
        try {
            decoded_w.assign(decoded_copy);
        } catch (...) {
        }
    }
    const std::string decoded_text = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_pending_async_decode_count > 0) {
            --dialog_pending_async_decode_count;
        }
        if (!dialog_shutdown_requested &&
            !dialog_callbacks_suspended &&
            map_state.map_ready &&
            req->decode_epoch == dialog_decode_epoch) {
            append_journal = true;
            if (active_dialog_cache.agent_id == req->agent_id &&
                active_dialog_body_decode_nonce == req->decode_nonce) {
                try {
                    active_dialog_cache.message = decoded_w;
                } catch (...) {
                }
            }
        }
    }
    dialog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    if (append_journal) {
        AppendDialogCallbackJournalEntry(
            req->tick,
            req->message_id,
            true,
            "recv_body",
            0,
            req->context_dialog_id,
            req->agent_id,
            false,
            req->context_dialog_id != 0,
            req->map_id,
            req->model_id,
            decoded_text
        );
    }
    ReleaseDialogBodyDecodeRequest(req);
}

void __cdecl OnDialogButtonDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogButtonDecodeRequest*>(param);
    if (!req) {
        return;
    }
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, false);
    wchar_t* decoded_copy = DupWideStringSafe(s);
    const std::string decoded_label = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    bool append_journal = false;
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_pending_async_decode_count > 0) {
            --dialog_pending_async_decode_count;
        }
        if (!dialog_shutdown_requested &&
            !dialog_callbacks_suspended &&
            map_state.map_ready &&
            req->decode_epoch == dialog_decode_epoch) {
            try {
                decoded_button_label_cache[req->dialog_id] = decoded_label;
                if (decoded_button_label_cache.size() > kMaxDecodedButtonLabelCache) {
                    decoded_button_label_cache.erase(decoded_button_label_cache.begin());
                }
                decoded_button_label_pending.erase(req->dialog_id);
                append_journal = true;
            } catch (...) {
                decoded_button_label_pending.erase(req->dialog_id);
            }
        }
    }
    dialog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    if (append_journal) {
        AppendDialogCallbackJournalEntry(
            req->tick,
            req->message_id,
            true,
            "recv_choice",
            req->dialog_id,
            req->context_dialog_id,
            req->agent_id,
            true,
            req->context_dialog_id != 0,
            req->map_id,
            req->model_id,
            decoded_label
        );
    }
    ReleaseDialogButtonDecodeRequest(req);
}

void __cdecl OnDialogTextDecoded(void* param, const wchar_t* s) {
    auto* req = static_cast<DialogDecodeRequest*>(param);
    if (!req) {
        return;
    }
    wchar_t* decoded_copy = DupWideStringSafe(s);
    const std::string decoded_text = decoded_copy ? WideToUtf8Safe(decoded_copy) : std::string{};
    {
        std::scoped_lock lock(catalog_mutex);
        if (catalog_pending_async_decode_count > 0) {
            --catalog_pending_async_decode_count;
        }
        if (!catalog_shutdown_requested && req->decode_epoch == catalog_decode_epoch) {
            try {
                decoded_text_cache[req->dialog_id] = decoded_text;
                decoded_text_pending.erase(req->dialog_id);
            } catch (...) {
                decoded_text_pending.erase(req->dialog_id);
            }
        }
    }
    catalog_async_decode_drained.notify_all();
    delete[] decoded_copy;
    ReleaseDialogDecodeRequest(req);
}

/* ---------------- Catalog decode queueing ---------------- */

void QueueDialogTextDecode(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return;
    }

    uint64_t request_epoch = 0;
    {
        std::scoped_lock lock(catalog_mutex);
        if (catalog_shutdown_requested) {
            return;
        }
        if (decoded_text_cache.find(dialog_id) != decoded_text_cache.end()) {
            return;
        }
        auto pending = decoded_text_pending.find(dialog_id);
        if (pending != decoded_text_pending.end() && pending->second) {
            return;
        }
        try {
            decoded_text_pending[dialog_id] = true;
        } catch (...) {
            decoded_text_pending.erase(dialog_id);
            return;
        }
        request_epoch = catalog_decode_epoch;
    }

    DialogMemory::DialogLoader_GetText_fn dialog_loader_get_text = ResolveDialogLoaderGetText();
    if (!dialog_loader_get_text) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    auto* encoded_ptr = static_cast<const wchar_t*>(SafeCallDialogLoader_GetText(dialog_loader_get_text, dialog_id));
    if (!encoded_ptr) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    wchar_t* encoded_copy = DupWideStringSafe(encoded_ptr);
    if (!encoded_copy) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = {};
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        return;
    }

    if (!SafeIsValidEncStr(encoded_copy)) {
        std::scoped_lock lock(catalog_mutex);
        if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
            try {
                decoded_text_cache[dialog_id] = WideToUtf8Safe(encoded_copy);
            } catch (...) {
            }
            decoded_text_pending.erase(dialog_id);
        }
        delete[] encoded_copy;
        return;
    }

    auto* req = new (std::nothrow) DialogDecodeRequest();
    if (!req) {
        {
            std::scoped_lock lock(catalog_mutex);
            if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
                decoded_text_pending.erase(dialog_id);
            }
        }
        delete[] encoded_copy;
        return;
    }
    req->dialog_id = dialog_id;
    req->decode_epoch = request_epoch;
    req->encoded = encoded_copy;

    {
        std::scoped_lock lock(catalog_mutex);
        if (catalog_shutdown_requested || req->decode_epoch != catalog_decode_epoch) {
            ReleaseDialogDecodeRequest(req);
            return;
        }
        ++catalog_pending_async_decode_count;
    }
    if (!SafeAsyncDecodeStr(req->encoded, OnDialogTextDecoded, req)) {
        {
            std::scoped_lock lock(catalog_mutex);
            if (catalog_pending_async_decode_count > 0) {
                --catalog_pending_async_decode_count;
            }
            if (request_epoch == catalog_decode_epoch && !catalog_shutdown_requested) {
                decoded_text_pending.erase(dialog_id);
            }
        }
        catalog_async_decode_drained.notify_all();
        ReleaseDialogDecodeRequest(req);
    }
}

void ClearCatalogCache() {
    std::scoped_lock lock(catalog_mutex);
    decoded_text_cache.clear();
    decoded_text_pending.clear();
    ++catalog_decode_epoch;
    InvalidateDialogTables();
}

/* ---------------- Hook install/teardown ---------------- */

void RegisterDialogUiHooks() {
    {
        std::scoped_lock lock(dialog_mutex);
        if (dialog_hook_registered) {
            return;
        }
    }
    GW::ui::RegisterUIMessageCallback(
        &dialog_ui_message_entry_body,
        UIMessage::kDialogBody,
        OnDialogUIMessage,
        0x1);
    GW::ui::RegisterUIMessageCallback(
        &dialog_ui_message_entry_button,
        UIMessage::kDialogButton,
        OnDialogUIMessage,
        0x1);
    GW::ui::RegisterUIMessageCallback(
        &dialog_ui_message_entry_send_agent,
        UIMessage::kSendAgentDialog,
        OnDialogUIMessage,
        0x1);
    GW::ui::RegisterUIMessageCallback(
        &dialog_ui_message_entry_send_gadget,
        UIMessage::kSendGadgetDialog,
        OnDialogUIMessage,
        0x1);
    std::scoped_lock lock(dialog_mutex);
    dialog_hook_registered = true;
}

void UnregisterDialogUiHooks() {
    {
        std::scoped_lock lock(dialog_mutex);
        if (!dialog_hook_registered) {
            return;
        }
        dialog_hook_registered = false;
    }
    if (!TryUnregisterDialogUiHooksRaw(
            &dialog_ui_message_entry_body,
            &dialog_ui_message_entry_button,
            &dialog_ui_message_entry_send_agent,
            &dialog_ui_message_entry_send_gadget)) {
        Logger::Instance().LogInfo("[dialog] Failed to remove one or more dialog UI hooks during shutdown.");
    }
}

}  // namespace

/* ---------------- Lifecycle ---------------- */

bool Initialize() {
    CrashContextScope context("startup", "dialog", "initialize");
    {
        std::scoped_lock lock(dialog_mutex);
        dialog_shutdown_requested = false;
    }
    {
        std::scoped_lock lock(catalog_mutex);
        catalog_shutdown_requested = false;
    }
    ClearCache();
    RegisterDialogUiHooks();
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "dialog", "shutdown");
    {
        std::scoped_lock lock(dialog_mutex);
        dialog_shutdown_requested = true;
        ++dialog_decode_epoch;
        ++active_dialog_body_decode_nonce;
    }
    UnregisterDialogUiHooks();
    {
        std::unique_lock lock(dialog_mutex);
        bool logged_wait = false;
        while (dialog_pending_async_decode_count != 0) {
            const bool drained = dialog_async_decode_drained.wait_for(
                lock,
                kDialogAsyncDrainTimeout,
                [] { return dialog_pending_async_decode_count == 0; });
            if (drained) {
                break;
            }
            if (!logged_wait) {
                Logger::Instance().LogInfo("[dialog] Async dialog decodes did not drain within the initial shutdown timeout; continuing to wait fail-closed.");
                logged_wait = true;
            }
        }
    }
    ClearCache();

    // Legacy DialogCatalog::Terminate: flag, drain, clear.
    {
        std::scoped_lock lock(catalog_mutex);
        catalog_shutdown_requested = true;
        ++catalog_decode_epoch;
    }
    {
        std::unique_lock lock(catalog_mutex);
        bool logged_wait = false;
        while (catalog_pending_async_decode_count != 0) {
            const bool drained = catalog_async_decode_drained.wait_for(
                lock,
                kDialogAsyncDrainTimeout,
                [] { return catalog_pending_async_decode_count == 0; });
            if (drained) {
                break;
            }
            if (!logged_wait) {
                Logger::Instance().LogInfo("[dialog] Async catalog decodes did not drain within the initial shutdown timeout; continuing to wait fail-closed.");
                logged_wait = true;
            }
        }
        decoded_text_cache.clear();
        decoded_text_pending.clear();
    }
}

void PollMapChange() {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    ObserveMapChange(map_state.map_id, map_state.map_ready, true);

    if (!map_state.map_ready || map_state.map_id == 0) {
        return;
    }

    const uint64_t now = GetTickCount64();
    std::scoped_lock lock(dialog_mutex);
    if (dialog_shutdown_requested || !dialog_callbacks_suspended) {
        return;
    }
    if (last_observed_map_id != map_state.map_id || !last_observed_map_ready) {
        return;
    }
    if (now < dialog_callbacks_resume_tick) {
        return;
    }
    dialog_callbacks_suspended = false;
    dialog_callbacks_resume_tick = 0;
}

/* ---------------- Catalog surface ---------------- */

bool IsDialogAvailable(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return false;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return false;
    }
    return (ReadDialogFlags(dialog_id) & 0x1) != 0;
}

DialogInfo GetDialogInfo(uint32_t dialog_id) {
    DialogInfo info = {dialog_id, 0, 0, 0, 0, 0, L"", 0};
    if (!IsDialogMapReadySafe()) {
        return info;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return info;
    }

    info.flags = ReadDialogFlags(dialog_id);
    info.frame_type = ReadDialogFrameType(dialog_id);
    info.event_handler = ReadDialogEventHandler(dialog_id);
    info.content_id = ReadDialogContentId(dialog_id);
    info.property_id = ReadDialogPropertyId(dialog_id);
    info.content = Utf8ToWideSafe(GetDialogTextDecoded(dialog_id));
    return info;
}

std::vector<DialogInfo> EnumerateAvailableDialogs() {
    std::vector<DialogInfo> dialogs;
    if (!IsDialogMapReadySafe()) {
        return dialogs;
    }
    dialogs.reserve(DialogMemory::MAX_DIALOG_ID + 1);
    for (uint32_t dialog_id = 0; dialog_id <= DialogMemory::MAX_DIALOG_ID; ++dialog_id) {
        if (IsDialogAvailable(dialog_id)) {
            dialogs.push_back(GetDialogInfo(dialog_id));
        }
    }
    return dialogs;
}

std::string GetDialogTextDecoded(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return {};
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return {};
    }
    {
        std::scoped_lock lock(catalog_mutex);
        auto it = decoded_text_cache.find(dialog_id);
        if (it != decoded_text_cache.end()) {
            return it->second;
        }
        auto pending = decoded_text_pending.find(dialog_id);
        if (pending != decoded_text_pending.end() && pending->second) {
            return {};
        }
    }
    QueueDialogTextDecode(dialog_id);
    return {};
}

bool IsDialogTextDecodePending(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return false;
    }
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_pending.find(dialog_id);
    return it != decoded_text_pending.end() && it->second;
}

std::vector<DialogTextDecodedInfo> GetDecodedDialogTextStatus() {
    if (!IsDialogMapReadySafe()) {
        return {};
    }
    std::scoped_lock lock(catalog_mutex);
    std::vector<DialogTextDecodedInfo> out;
    out.reserve(decoded_text_cache.size() + decoded_text_pending.size());
    for (const auto& kv : decoded_text_cache) {
        DialogTextDecodedInfo info{};
        info.dialog_id = kv.first;
        info.text = kv.second;
        info.pending = false;
        out.push_back(std::move(info));
    }
    for (const auto& kv : decoded_text_pending) {
        if (!kv.second || decoded_text_cache.find(kv.first) != decoded_text_cache.end()) {
            continue;
        }
        DialogTextDecodedInfo info{};
        info.dialog_id = kv.first;
        info.pending = true;
        out.push_back(std::move(info));
    }
    return out;
}

bool TryGetCachedDialogTextDecoded(uint32_t dialog_id, std::string& out) {
    if (!IsDialogMapReadySafe()) {
        return false;
    }
    std::scoped_lock lock(catalog_mutex);
    auto it = decoded_text_cache.find(dialog_id);
    if (it == decoded_text_cache.end()) {
        return false;
    }
    out = it->second;
    return true;
}

uint32_t ReadDialogFlags(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return 0;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.flags_base) {
        return 0;
    }
    const uintptr_t address = tables.flags_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t flags = 0;
    if (!TryReadU32(address, flags, "FLAGS")) {
        return 0;
    }
    return flags;
}

uint32_t ReadDialogFrameType(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return 0;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.frame_type_base) {
        return 0;
    }
    const uintptr_t address = tables.frame_type_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t frame_type = 0;
    if (!TryReadU32(address, frame_type, "FRAME_TYPE")) {
        return 0;
    }
    return frame_type;
}

uint32_t ReadDialogEventHandler(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return 0;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.event_handler_base) {
        return 0;
    }
    const uintptr_t address = tables.event_handler_base + (dialog_id * DialogMemory::FLAGS_STRIDE);
    uint32_t handler = 0;
    if (!TryReadU32(address, handler, "EVENT_HANDLER")) {
        return 0;
    }
    return handler;
}

uint32_t ReadDialogContentId(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return 0;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.content_id_base) {
        return 0;
    }
    const uintptr_t address = tables.content_id_base + (dialog_id * DialogMemory::CONTENT_STRIDE);
    uint32_t content_id = 0;
    if (!TryReadU32(address, content_id, "CONTENT_ID")) {
        return 0;
    }
    return content_id;
}

uint32_t ReadDialogPropertyId(uint32_t dialog_id) {
    if (!IsDialogMapReadySafe()) {
        return 0;
    }
    if (dialog_id > DialogMemory::MAX_DIALOG_ID) {
        return 0;
    }
    const auto& tables = GetDialogTables();
    if (!tables.property_id_base) {
        return 0;
    }
    const uintptr_t address = tables.property_id_base + (dialog_id * DialogMemory::PROPERTY_STRIDE);
    uint32_t property_id = 0;
    if (!TryReadU32(address, property_id, "PROPERTY_ID")) {
        return 0;
    }
    return property_id;
}

/* ---------------- Runtime surface ---------------- */

uint32_t GetLastSelectedDialogId() {
    std::scoped_lock lock(dialog_mutex);
    return last_selected_dialog_id;
}

ActiveDialogInfo GetActiveDialog() {
    std::scoped_lock lock(dialog_mutex);
    return active_dialog_cache;
}

std::vector<DialogButtonInfo> GetActiveDialogButtons() {
    std::vector<DialogButtonInfo> out;
    {
        std::scoped_lock lock(dialog_mutex);
        out = active_dialog_buttons;
    }
    for (auto& btn : out) {
        if (btn.dialog_id == 0) {
            continue;
        }
        std::string label;
        bool pending = false;
        {
            std::scoped_lock lock(dialog_mutex);
            auto it = decoded_button_label_cache.find(btn.dialog_id);
            if (it != decoded_button_label_cache.end()) {
                label = it->second;
            }
            auto pit = decoded_button_label_pending.find(btn.dialog_id);
            if (pit != decoded_button_label_pending.end()) {
                pending = pit->second;
            }
        }
        if (label.empty()) {
            label = GetDialogTextDecoded(btn.dialog_id);
            pending = IsDialogTextDecodePending(btn.dialog_id);
        }
        btn.message_decoded = label;
        btn.message = label;
        btn.message_decode_pending = pending;
    }
    return out;
}

bool IsDialogActive() {
    // "NPC Dialog" root frame hash used across Py4GW UI helpers.
    constexpr uint32_t kNpcDialogHash = 3856160816u;
    __try {
        const uint32_t frame_id = GW::ui::GetFrameIDByHash(kNpcDialogHash);
        if (!frame_id) {
            return false;
        }
        const GW::ui::Frame* frame = GW::ui::GetFrameById(frame_id);
        if (!frame) {
            return false;
        }
        return frame->IsCreated() && frame->IsVisible();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsDialogDisplayed(uint32_t dialog_id) {
    std::scoped_lock lock(dialog_mutex);
    if (dialog_id == 0) {
        return false;
    }
    return active_dialog_cache.dialog_id == dialog_id ||
        active_dialog_cache.context_dialog_id == dialog_id;
}

std::vector<DialogEventLog> GetDialogEventLogs() {
    std::scoped_lock lock(dialog_mutex);
    return dialog_event_logs;
}

std::vector<DialogEventLog> GetDialogEventLogsReceived() {
    std::scoped_lock lock(dialog_mutex);
    return dialog_event_logs_received;
}

std::vector<DialogEventLog> GetDialogEventLogsSent() {
    std::scoped_lock lock(dialog_mutex);
    return dialog_event_logs_sent;
}

void ClearDialogEventLogs() {
    std::scoped_lock lock(dialog_mutex);
    dialog_event_logs.clear();
    dialog_event_logs_received.clear();
    dialog_event_logs_sent.clear();
}

void ClearDialogEventLogsReceived() {
    std::scoped_lock lock(dialog_mutex);
    dialog_event_logs_received.clear();
}

void ClearDialogEventLogsSent() {
    std::scoped_lock lock(dialog_mutex);
    dialog_event_logs_sent.clear();
}

std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournal() {
    std::vector<DialogCallbackJournalEntry> entries;
    {
        std::scoped_lock lock(dialog_mutex);
        entries = dialog_callback_journal;
    }
    return SortDialogCallbackJournalEntries(entries);
}

std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalReceived() {
    std::vector<DialogCallbackJournalEntry> entries;
    {
        std::scoped_lock lock(dialog_mutex);
        entries = dialog_callback_journal_received;
    }
    return SortDialogCallbackJournalEntries(entries);
}

std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalSent() {
    std::vector<DialogCallbackJournalEntry> entries;
    {
        std::scoped_lock lock(dialog_mutex);
        entries = dialog_callback_journal_sent;
    }
    return SortDialogCallbackJournalEntries(entries);
}

void ClearDialogCallbackJournal() {
    std::scoped_lock lock(dialog_mutex);
    dialog_callback_journal.clear();
    dialog_callback_journal_received.clear();
    dialog_callback_journal_sent.clear();
}

void ClearDialogCallbackJournalReceived() {
    std::scoped_lock lock(dialog_mutex);
    dialog_callback_journal_received.clear();
}

void ClearDialogCallbackJournalSent() {
    std::scoped_lock lock(dialog_mutex);
    dialog_callback_journal_sent.clear();
}

void ClearDialogCallbackJournalFiltered(
    std::optional<std::string> npc_uid,
    std::optional<bool> incoming,
    std::optional<uint32_t> message_id,
    std::optional<std::string> event_type
) {
    std::scoped_lock lock(dialog_mutex);
    if (!npc_uid.has_value() && !incoming.has_value() && !message_id.has_value() && !event_type.has_value()) {
        dialog_callback_journal.clear();
        dialog_callback_journal_received.clear();
        dialog_callback_journal_sent.clear();
        return;
    }

    const bool has_event_type = event_type.has_value() && !event_type->empty();
    const std::string event_type_filter = has_event_type ? *event_type : "";
    auto matches = [&](const DialogCallbackJournalEntry& entry) -> bool {
        if (npc_uid.has_value() && entry.npc_uid != *npc_uid) {
            return false;
        }
        if (incoming.has_value() && entry.incoming != *incoming) {
            return false;
        }
        if (message_id.has_value() && entry.message_id != *message_id) {
            return false;
        }
        if (has_event_type && entry.event_type != event_type_filter) {
            return false;
        }
        return true;
    };

    dialog_callback_journal.erase(
        std::remove_if(dialog_callback_journal.begin(), dialog_callback_journal.end(), matches),
        dialog_callback_journal.end());

    dialog_callback_journal_received.clear();
    dialog_callback_journal_sent.clear();
    dialog_callback_journal_received.reserve(dialog_callback_journal.size());
    dialog_callback_journal_sent.reserve(dialog_callback_journal.size());
    for (const auto& entry : dialog_callback_journal) {
        if (entry.incoming) {
            dialog_callback_journal_received.push_back(entry);
        }
        else {
            dialog_callback_journal_sent.push_back(entry);
        }
    }
}

void ClearCache() {
    const DialogMapStateSnapshot map_state = GetDialogMapStateSafe();
    const uint64_t now = GetTickCount64();
    {
        std::scoped_lock lock(dialog_mutex);
        active_dialog_cache = {0, 0, 0, false, L""};
        active_dialog_buttons.clear();
        last_selected_dialog_id = 0;
        pending_body_context_dialog_id = 0;
        pending_body_context_agent_id = 0;
        decoded_button_label_cache.clear();
        decoded_button_label_pending.clear();
        dialog_event_logs.clear();
        dialog_event_logs_received.clear();
        dialog_event_logs_sent.clear();
        dialog_callback_journal.clear();
        dialog_callback_journal_received.clear();
        dialog_callback_journal_sent.clear();
        ++dialog_decode_epoch;
        ++active_dialog_body_decode_nonce;
        last_observed_map_id = map_state.map_id;
        last_observed_map_ready = map_state.map_ready;
        dialog_callbacks_suspended = !map_state.map_ready;
        dialog_callbacks_resume_tick = map_state.map_ready ? 0 : now;
    }
    ClearCatalogCache();
}

}  // namespace GW::dialog
