#include "base/error_handling.h"

#include "GW/quest/quest.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

bool ResolveRequestQuestFunctions() {
    CrashContextScope context("startup", "quest", "resolve_request_quest_functions");
    const auto* anchor_pattern = PY4GW::Patterns::Get("quest.request_anchor");
    const auto* call_pattern = PY4GW::Patterns::Get("quest.request_data_call");
    if (!anchor_pattern || !call_pattern) {
        Logger::Instance().LogError("Missing or invalid quest request pattern.", "quest");
        return false;
    }

    const uintptr_t anchor = PY4GW::Scanner::Find(
        anchor_pattern->pattern.c_str(),
        anchor_pattern->mask.c_str(),
        anchor_pattern->offset,
        anchor_pattern->section);
    if (!Logger::AssertAddress("RequestQuest_Anchor", anchor, "quest")) {
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::FindInRange(
        call_pattern->pattern.c_str(),
        call_pattern->mask.c_str(),
        call_pattern->offset,
        anchor,
        anchor + anchor_pattern->range);
    if (!Logger::AssertAddress("RequestQuestData_Callsite", callsite, "quest")) {
        return false;
    }

    GW::quest::g_request_quest_data_func = reinterpret_cast<GW::quest::RequestQuestDataFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    if (!Logger::AssertAddress(
            "RequestQuestData_Func",
            reinterpret_cast<uintptr_t>(GW::quest::g_request_quest_data_func),
            "quest")) {
        return false;
    }

    GW::quest::g_request_quest_info_func = reinterpret_cast<GW::quest::RequestQuestInfoFn>(
        PY4GW::Scanner::ToFunctionStart(anchor, 0xFF));
    return Logger::AssertAddress(
        "RequestQuestInfo_Func",
        reinterpret_cast<uintptr_t>(GW::quest::g_request_quest_info_func),
        "quest");
}

bool Init() {
    CrashContextScope context("startup", "quest", "init");
    return ResolveRequestQuestFunctions();
}

void Exit() {
    CrashContextScope context("shutdown", "quest", "exit");
    GW::quest::g_request_quest_info_func = nullptr;
    GW::quest::g_request_quest_data_func = nullptr;
}

}  // namespace

namespace GW::quest {

bool Initialize() {
    CrashContextScope context("startup", "quest", "initialize");
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
    CrashContextScope context("shutdown", "quest", "shutdown");
    if (!g_initialized) {
        return;
    }

    Exit();
    g_initialized = false;
}

}  // namespace GW::quest
