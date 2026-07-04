#include "base/error_handling.h"

#include "GW/dialog/dialog.h"

#include "base/CrashHandler.h"
#include "base/logger.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

// Resolution of the module-owned dialog symbols. These are hardcoded client
// VAs rebased onto the live module with a validation pass, plus a heuristic
// .rdata fallback scan (legacy DialogCatalog mechanism kept by user direction;
// the JSON pattern system has no module-base-relative op to express them).
namespace GW::dialog {

namespace {

constexpr uintptr_t kGwImageBase = 0x00400000;

DialogTableAddrs g_dialog_tables;

uintptr_t ToRuntimeAddress(uintptr_t va) {
    static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) {
        return va;
    }
    return base + (va - kGwImageBase);
}

struct SectionRange {
    uintptr_t start = 0;
    uintptr_t end = 0;

    bool valid() const {
        return start && end && end > start;
    }
};

bool TryReadU32NoLog(uintptr_t address, uint32_t& out) {
    __try {
        out = *reinterpret_cast<uint32_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        return false;
    }
}

bool GetModuleInfo(uintptr_t& base) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module) {
        base = 0;
        return false;
    }
    base = reinterpret_cast<uintptr_t>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    return true;
}

SectionRange GetSectionRange(const char* name) {
    SectionRange out{};
    uintptr_t base = 0;
    if (!GetModuleInfo(base)) {
        return out;
    }
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& sec = sections[i];
        char sec_name[9] = {};
        std::memcpy(sec_name, sec.Name, 8);
        if (std::strncmp(sec_name, name, 8) == 0) {
            const uintptr_t start = base + sec.VirtualAddress;
            const uintptr_t end = start + std::max(sec.Misc.VirtualSize, sec.SizeOfRawData);
            out.start = start;
            out.end = end;
            return out;
        }
    }
    return out;
}

uintptr_t ResolveFlagsBase(const SectionRange& rdata, const SectionRange& text) {
    if (!rdata.valid() || !text.valid()) {
        return 0;
    }
    const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
    const size_t stride = DialogMemory::FLAGS_STRIDE;
    const uintptr_t start = rdata.start + 8;
    const uintptr_t end = rdata.end - (count * stride);
    for (uintptr_t addr = start; addr + count * stride <= end; addr += 4) {
        bool ok = true;
        size_t enabled = 0;
        for (size_t i = 0; i < count; ++i) {
            uint32_t flags = 0;
            if (!TryReadU32NoLog(addr + i * stride, flags)) {
                ok = false;
                break;
            }
            if (flags > 0xFFFF) {
                ok = false;
                break;
            }
            if (flags & 0x1) {
                ++enabled;
            }
            uint32_t handler = 0;
            if (!TryReadU32NoLog(addr - 8 + i * stride, handler)) {
                ok = false;
                break;
            }
            if (handler != 0 && (handler < text.start || handler >= text.end)) {
                ok = false;
                break;
            }
        }
        if (ok && enabled > 0) {
            return addr;
        }
    }
    return 0;
}

bool ValidateDialogMetadataBases(
    uintptr_t flags_base,
    uintptr_t frame_type_base,
    uintptr_t event_handler_base,
    uintptr_t content_id_base,
    uintptr_t property_id_base,
    const SectionRange& text) {
    if (!flags_base || !frame_type_base || !event_handler_base || !content_id_base || !property_id_base || !text.valid()) {
        return false;
    }

    const size_t count = DialogMemory::MAX_DIALOG_ID + 1;
    size_t enabled = 0;
    for (size_t i = 0; i < count; ++i) {
        const uintptr_t offset = i * DialogMemory::FLAGS_STRIDE;
        uint32_t flags = 0;
        uint32_t handler = 0;
        uint32_t frame_type = 0;
        uint32_t content_id = 0;
        uint32_t property_id = 0;
        if (!TryReadU32NoLog(flags_base + offset, flags) ||
            !TryReadU32NoLog(event_handler_base + offset, handler) ||
            !TryReadU32NoLog(frame_type_base + offset, frame_type) ||
            !TryReadU32NoLog(content_id_base + i * DialogMemory::CONTENT_STRIDE, content_id) ||
            !TryReadU32NoLog(property_id_base + i * DialogMemory::PROPERTY_STRIDE, property_id)) {
            return false;
        }
        if (flags > 0xFFFF) {
            return false;
        }
        if (handler != 0 && (handler < text.start || handler >= text.end)) {
            return false;
        }
        if (flags & 0x1) {
            ++enabled;
        }
    }
    return enabled > 0;
}

DialogTableAddrs BuildStaticDialogTables(const SectionRange& text) {
    DialogTableAddrs tables{};
    tables.flags_base = ToRuntimeAddress(DialogMemory::FLAGS_BASE);
    tables.frame_type_base = ToRuntimeAddress(DialogMemory::FRAME_TYPE_BASE);
    tables.event_handler_base = ToRuntimeAddress(DialogMemory::EVENT_HANDLER_BASE);
    tables.content_id_base = ToRuntimeAddress(DialogMemory::CONTENT_ID_BASE);
    tables.property_id_base = ToRuntimeAddress(DialogMemory::PROPERTY_ID_BASE);

    if (!ValidateDialogMetadataBases(
            tables.flags_base,
            tables.frame_type_base,
            tables.event_handler_base,
            tables.content_id_base,
            tables.property_id_base,
            text)) {
        tables.flags_base = 0;
        tables.frame_type_base = 0;
        tables.event_handler_base = 0;
        tables.content_id_base = 0;
        tables.property_id_base = 0;
    }
    return tables;
}

DialogTableAddrs BuildResolvedDialogTables(
    const SectionRange& rdata,
    const SectionRange& text) {
    DialogTableAddrs tables{};
    tables.flags_base = ResolveFlagsBase(rdata, text);
    if (tables.flags_base) {
        tables.event_handler_base = tables.flags_base - 0x8;
        tables.frame_type_base = tables.flags_base - 0x4;
        tables.content_id_base = tables.flags_base + 0x4;
        tables.property_id_base = tables.flags_base + 0x8;
    }

    if (!ValidateDialogMetadataBases(
            tables.flags_base,
            tables.frame_type_base,
            tables.event_handler_base,
            tables.content_id_base,
            tables.property_id_base,
            text)) {
        tables.flags_base = 0;
        tables.frame_type_base = 0;
        tables.event_handler_base = 0;
        tables.content_id_base = 0;
        tables.property_id_base = 0;
    }
    return tables;
}

}  // namespace

DialogTableAddrs& GetDialogTables() {
    if (g_dialog_tables.resolved) {
        return g_dialog_tables;
    }
    CrashContextScope context("runtime", "dialog", "resolve_dialog_tables");
    g_dialog_tables.resolved = true;

    const SectionRange rdata = GetSectionRange(".rdata");
    const SectionRange text = GetSectionRange(".text");

    g_dialog_tables = BuildStaticDialogTables(text);
    if (!g_dialog_tables.flags_base) {
        DialogTableAddrs resolved = BuildResolvedDialogTables(rdata, text);
        g_dialog_tables.flags_base = resolved.flags_base;
        g_dialog_tables.frame_type_base = resolved.frame_type_base;
        g_dialog_tables.event_handler_base = resolved.event_handler_base;
        g_dialog_tables.content_id_base = resolved.content_id_base;
        g_dialog_tables.property_id_base = resolved.property_id_base;
    }
    g_dialog_tables.resolved = true;

    if (!g_dialog_tables.flags_base) {
        Logger::Instance().LogInfo("[dialog] Dialog table resolution incomplete. Some dialog metadata may be unavailable.");
    }

    return g_dialog_tables;
}

void InvalidateDialogTables() {
    g_dialog_tables = {};
}

DialogMemory::DialogLoader_GetText_fn ResolveDialogLoaderGetText() {
    static DialogMemory::DialogLoader_GetText_fn cached = nullptr;
    if (cached) {
        return cached;
    }
    cached = reinterpret_cast<DialogMemory::DialogLoader_GetText_fn>(
        ToRuntimeAddress(DialogMemory::DIALOG_LOADER_GETTEXT));
    return cached;
}

}  // namespace GW::dialog
