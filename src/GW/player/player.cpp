#include "base/error_handling.h"

#include "GW/player/player.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

bool ResolveSetActiveTitle() {
    CrashContextScope context("startup", "player", "resolve_set_active_title");
    const auto* anchor_pattern = PY4GW::Patterns::Get("player.set_active_title_anchor");
    const auto* call_pattern = PY4GW::Patterns::Get("player.set_active_title_call");
    if (!anchor_pattern || !call_pattern) {
        Logger::Instance().LogError("Missing or invalid set active title pattern.", "player");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        anchor_pattern->assertion_file.c_str(),
        anchor_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(anchor_pattern->line_number),
        anchor_pattern->offset);
    if (!Logger::AssertAddress("SetActiveTitle_Anchor", address, "player")) {
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        call_pattern->pattern.c_str(),
        call_pattern->mask.c_str(),
        call_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("SetActiveTitle_Callsite", address, "player")) {
        return false;
    }

    GW::player::g_set_active_title_func = reinterpret_cast<GW::player::SetActiveTitleFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress(
        "SetActiveTitle_Func",
        reinterpret_cast<uintptr_t>(GW::player::g_set_active_title_func),
        "player");
}

bool ResolveRemoveActiveTitle() {
    CrashContextScope context("startup", "player", "resolve_remove_active_title");
    if (!GW::player::g_set_active_title_func) {
        Logger::Instance().LogError("Set active title function must resolve before remove active title.", "player");
        return false;
    }

    const auto* pattern = PY4GW::Patterns::Get("player.remove_active_title_signature");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.remove_active_title_signature", "player");
        return false;
    }

    uintptr_t address = reinterpret_cast<uintptr_t>(GW::player::g_set_active_title_func);
    address = PY4GW::Scanner::FindInRange(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        address + 0x10,
        address + 0xFF);
    GW::player::g_remove_active_title_func = reinterpret_cast<GW::player::RemoveActiveTitleFn>(address);
    return Logger::AssertAddress(
        "RemoveActiveTitle_Func",
        reinterpret_cast<uintptr_t>(GW::player::g_remove_active_title_func),
        "player");
}

bool ResolveDepositFaction() {
    CrashContextScope context("startup", "player", "resolve_deposit_faction");
    const auto* pattern = PY4GW::Patterns::Get("player.deposit_faction_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.deposit_faction_callsite", "player");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DepositFaction_Callsite", callsite, "player")) {
        return false;
    }

    GW::player::g_deposit_faction_func = reinterpret_cast<GW::player::DepositFactionFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress(
        "DepositFaction_Func",
        reinterpret_cast<uintptr_t>(GW::player::g_deposit_faction_func),
        "player");
}

bool ResolveTitleData() {
    CrashContextScope context("startup", "player", "resolve_title_data");
    const auto* pattern = PY4GW::Patterns::Get("player.title_data_ref");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: player.title_data_ref", "player");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("TitleData_Ref", address, "player")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("TitleData_Ptr", candidate, "player")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate, PY4GW::ScannerSection::RData)) {
        Logger::Instance().LogError("Title data pointer is outside the expected rdata section.", "player");
        return false;
    }

    GW::player::g_title_data = reinterpret_cast<GW::Context::TitleClientData*>(candidate);
    return true;
}

bool Init() {
    CrashContextScope context("startup", "player", "init");
    return ResolveSetActiveTitle() &&
        ResolveRemoveActiveTitle() &&
        ResolveDepositFaction() &&
        ResolveTitleData();
}

void Exit() {
    CrashContextScope context("shutdown", "player", "exit");
    GW::player::g_remove_active_title_func = nullptr;
    GW::player::g_set_active_title_func = nullptr;
    GW::player::g_deposit_faction_func = nullptr;
    GW::player::g_title_data = nullptr;
}

}  // namespace

namespace GW::player {

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

    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "player", "shutdown");
    if (!g_initialized) {
        return;
    }

    Exit();
    g_initialized = false;
}

}  // namespace GW::player
