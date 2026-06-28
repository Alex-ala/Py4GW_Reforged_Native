#include "base/error_handling.h"

#include "GW/events/events.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

uint32_t __cdecl OnSendEventMessage(
    void* event_context,
    uint32_t unk1,
    GW::events::EventID event_id,
    void* data_buffer,
    uint32_t data_length) {
    PY4GW::HookBase::EnterHook();
    PY4GW::HookStatus status = {};
    uint32_t result = 1;

    auto found = GW::events::g_callbacks.find(event_id);
    if (found == GW::events::g_callbacks.end()) {
        PY4GW::HookBase::LeaveHook();
        return GW::events::g_send_event_message_original
            ? GW::events::g_send_event_message_original(event_context, unk1, event_id, data_buffer, data_length)
            : result;
    }

    auto it = found->second.begin();
    const auto end = found->second.end();
    while (it != end) {
        if (it->altitude > 0) {
            break;
        }
        it->callback(&status, event_id, data_buffer, data_length);
        ++status.altitude;
        ++it;
    }

    if (!status.blocked && GW::events::g_send_event_message_original) {
        result = GW::events::g_send_event_message_original(event_context, unk1, event_id, data_buffer, data_length);
    }

    while (it != end) {
        it->callback(&status, event_id, data_buffer, data_length);
        ++status.altitude;
        ++it;
    }

    PY4GW::HookBase::LeaveHook();
    return result;
}

bool ResolveSendEventMessageTarget() {
    CrashContextScope context("startup", "events", "resolve_send_event_message_target");
    const auto* pattern = PY4GW::Patterns::Get("events.send_event_message_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: events.send_event_message_callsite", "events");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("SendEventMessage_Callsite", callsite, "events")) {
        return false;
    }

    GW::events::g_send_event_message_func = reinterpret_cast<GW::events::SendEventMessageFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress(
        "SendEventMessage_Func",
        reinterpret_cast<uintptr_t>(GW::events::g_send_event_message_func),
        "events");
}

bool Init() {
    CrashContextScope context("startup", "events", "init");
    if (!ResolveSendEventMessageTarget()) {
        return false;
    }

    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&GW::events::g_send_event_message_func),
        reinterpret_cast<void*>(&OnSendEventMessage),
        reinterpret_cast<void**>(&GW::events::g_send_event_message_original));
    return Logger::AssertHook("SendEventMessage_Func", status, "events");
}

void EnableHooks() {
    CrashContextScope context("runtime", "events", "enable_hooks");
    // Legacy GWCA currently keeps this hook disabled.
    return;
}

void DisableHooks() {
    CrashContextScope context("shutdown", "events", "disable_hooks");
    if (GW::events::g_send_event_message_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(GW::events::g_send_event_message_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "events", "exit");
    if (GW::events::g_send_event_message_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(GW::events::g_send_event_message_func));
    }

    GW::events::g_send_event_message_func = nullptr;
    GW::events::g_send_event_message_original = nullptr;
    GW::events::g_callbacks.clear();
}

}  // namespace

namespace GW::events {

bool Initialize() {
    CrashContextScope context("startup", "events", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    PY4GW::HookBase::Initialize();
    if (!Init()) {
        Exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    EnableHooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "events", "shutdown");
    if (!g_initialized) {
        return;
    }

    DisableHooks();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::events
