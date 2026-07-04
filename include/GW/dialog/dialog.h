#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Dialog observer/catalog migrated from legacy py_dialog.h/.cpp and
// py_dialog_catalog.h/.cpp. The legacy Dialog and DialogCatalog static classes
// were merged into this single module by user direction (Dialog already
// delegated every catalog query); the class surface became the GW module
// namespace-function baseline. Internally the module keeps the legacy TWO-lock
// design (runtime dialog state vs. decoded-text catalog cache) because the
// send-dialog UI handler queries the catalog cache while holding the runtime
// lock - a single merged mutex would self-deadlock.
namespace GW::dialog {

struct DialogInfo {
    uint32_t dialog_id = 0;
    uint32_t flags = 0;
    uint32_t frame_type = 0;
    uint32_t event_handler = 0;
    uint32_t content_id = 0;
    uint32_t property_id = 0;
    std::wstring content = L"";
    uint32_t agent_id = 0;
};

struct ActiveDialogInfo {
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
    bool dialog_id_authoritative = false;
    std::wstring message = L"";
};

struct DialogButtonInfo {
    uint32_t dialog_id = 0;
    uint32_t button_icon = 0;
    std::string message = "";
    std::string message_decoded = "";
    bool message_decode_pending = false;
};

struct DialogTextDecodedInfo {
    uint32_t dialog_id = 0;
    std::string text = "";
    bool pending = false;
};

struct DialogEventLog {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    bool incoming = false;
    bool is_frame_message = false;
    uint32_t frame_id = 0;
    std::vector<uint8_t> w_bytes;
    std::vector<uint8_t> l_bytes;
};

struct DialogCallbackJournalEntry {
    uint64_t tick = 0;
    uint32_t message_id = 0;
    bool incoming = false;
    uint32_t dialog_id = 0;
    uint32_t context_dialog_id = 0;
    uint32_t agent_id = 0;
    uint32_t map_id = 0;
    uint32_t model_id = 0;
    bool dialog_id_authoritative = false;
    bool context_dialog_id_inferred = false;
    std::string npc_uid = "";
    std::string event_type = "";
    std::string text = "";
};

/* ---------------- Resolved-symbol surface (module-owned) ---------------- */
// The dialog metadata tables and DialogLoader_GetText are HARDCODED client
// virtual addresses rebased onto the live module, with a validation pass and
// a heuristic .rdata fallback scan (legacy DialogCatalog mechanism, kept by
// user direction). They cannot move into offsets/*.json because the pattern
// system has no module-base-relative op; resolution bodies live in
// dialog_patterns.cpp.
namespace DialogMemory {
    using DialogLoader_GetText_fn = void* (__cdecl*)(uint32_t dialog_id);

    constexpr uint32_t MAX_DIALOG_ID = 0x39u;
    constexpr uint32_t FLAGS_STRIDE = 0x24u;
    constexpr uint32_t CONTENT_STRIDE = 0x24u;
    constexpr uint32_t PROPERTY_STRIDE = 0x24u;

    constexpr uintptr_t EVENT_HANDLER_BASE = 0x00913918u;
    constexpr uintptr_t FRAME_TYPE_BASE = 0x0091391Cu;
    constexpr uintptr_t FLAGS_BASE = 0x00913920u;
    constexpr uintptr_t CONTENT_ID_BASE = 0x00913924u;
    constexpr uintptr_t PROPERTY_ID_BASE = 0x00913928u;
    constexpr uintptr_t DIALOG_LOADER_GETTEXT = 0x0079EEF0u;
}

struct DialogTableAddrs {
    uintptr_t flags_base = 0;
    uintptr_t frame_type_base = 0;
    uintptr_t event_handler_base = 0;
    uintptr_t content_id_base = 0;
    uintptr_t property_id_base = 0;
    bool resolved = false;
};

// Resolver ownership (bodies in dialog_patterns.cpp). GetDialogTables resolves
// lazily on first use; InvalidateDialogTables forces re-resolution (ClearCache).
DialogTableAddrs& GetDialogTables();
void InvalidateDialogTables();
DialogMemory::DialogLoader_GetText_fn ResolveDialogLoaderGetText();

/* ---------------- Lifecycle ---------------- */

bool Initialize();
void Shutdown();
// Called from the runtime update loop; resumes dialog callbacks after a map
// transition settles (legacy Dialog::PollMapChange).
void PollMapChange();

/* ---------------- Catalog surface (legacy DialogCatalog) ---------------- */

bool IsDialogAvailable(uint32_t dialog_id);
DialogInfo GetDialogInfo(uint32_t dialog_id);
std::vector<DialogInfo> EnumerateAvailableDialogs();

std::string GetDialogTextDecoded(uint32_t dialog_id);
bool IsDialogTextDecodePending(uint32_t dialog_id);
std::vector<DialogTextDecodedInfo> GetDecodedDialogTextStatus();
bool TryGetCachedDialogTextDecoded(uint32_t dialog_id, std::string& out);

uint32_t ReadDialogFlags(uint32_t dialog_id);
uint32_t ReadDialogFrameType(uint32_t dialog_id);
uint32_t ReadDialogEventHandler(uint32_t dialog_id);
uint32_t ReadDialogContentId(uint32_t dialog_id);
uint32_t ReadDialogPropertyId(uint32_t dialog_id);

/* ---------------- Runtime surface (legacy Dialog) ---------------- */

uint32_t GetLastSelectedDialogId();
ActiveDialogInfo GetActiveDialog();
std::vector<DialogButtonInfo> GetActiveDialogButtons();
bool IsDialogActive();
bool IsDialogDisplayed(uint32_t dialog_id);

std::vector<DialogEventLog> GetDialogEventLogs();
std::vector<DialogEventLog> GetDialogEventLogsReceived();
std::vector<DialogEventLog> GetDialogEventLogsSent();
void ClearDialogEventLogs();
void ClearDialogEventLogsReceived();
void ClearDialogEventLogsSent();

std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournal();
std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalReceived();
std::vector<DialogCallbackJournalEntry> GetDialogCallbackJournalSent();
void ClearDialogCallbackJournal();
void ClearDialogCallbackJournalReceived();
void ClearDialogCallbackJournalSent();
void ClearDialogCallbackJournalFiltered(
    std::optional<std::string> npc_uid = std::nullopt,
    std::optional<bool> incoming = std::nullopt,
    std::optional<uint32_t> message_id = std::nullopt,
    std::optional<std::string> event_type = std::nullopt);

// Clears both the runtime dialog state and the catalog decode cache (merged
// legacy Dialog::ClearCache + DialogCatalog::ClearCache).
void ClearCache();

}  // namespace GW::dialog
