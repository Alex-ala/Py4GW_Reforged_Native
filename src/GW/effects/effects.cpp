#include "base/error_handling.h"

#include "GW/effects/effects.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

bool ResolvePostProcessEffect() {
    CrashContextScope context("startup", "effects", "resolve_post_process_effect");
    const auto* pattern = PY4GW::Patterns::Get("effects.post_process_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.post_process_target", "effects");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    GW::effects::g_post_process_effect_func = reinterpret_cast<GW::effects::PostProcessEffectFn>(address);
    return Logger::AssertAddress(
        "PostProcessEffect_Func",
        reinterpret_cast<uintptr_t>(GW::effects::g_post_process_effect_func),
        "effects");
}

bool ResolveDropBuff() {
    CrashContextScope context("startup", "effects", "resolve_drop_buff");
    const auto* pattern = PY4GW::Patterns::Get("effects.drop_buff_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.drop_buff_callsite", "effects");
        return false;
    }

    const uintptr_t callsite = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DropBuff_Callsite", callsite, "effects")) {
        return false;
    }

    GW::effects::g_drop_buff_func = reinterpret_cast<GW::effects::DropBuffFn>(
        PY4GW::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress("DropBuff_Func", reinterpret_cast<uintptr_t>(GW::effects::g_drop_buff_func), "effects");
}

void __cdecl OnPostProcessEffect(uint32_t intensity, uint32_t tint) {
    PY4GW::HookBase::EnterHook();
    GW::effects::g_alcohol_level = intensity;

    if (GW::effects::g_post_process_effect_original) {
        GW::effects::g_post_process_effect_original(intensity, tint);
    }

    PY4GW::HookBase::LeaveHook();
}

bool Init() {
    CrashContextScope context("startup", "effects", "init");

    if (!ResolvePostProcessEffect() || !ResolveDropBuff()) {
        return false;
    }

    const int status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&GW::effects::g_post_process_effect_func),
        reinterpret_cast<void*>(&OnPostProcessEffect),
        reinterpret_cast<void**>(&GW::effects::g_post_process_effect_original));
    return Logger::AssertHook("PostProcessEffect_Func", status, "effects");
}

void EnableHooks() {
    CrashContextScope context("runtime", "effects", "enable_hooks");
    if (GW::effects::g_post_process_effect_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(GW::effects::g_post_process_effect_func));
    }
}

void DisableHooks() {
    CrashContextScope context("shutdown", "effects", "disable_hooks");
    if (GW::effects::g_post_process_effect_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(GW::effects::g_post_process_effect_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "effects", "exit");
    if (GW::effects::g_post_process_effect_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(GW::effects::g_post_process_effect_func));
    }

    GW::effects::g_post_process_effect_func = nullptr;
    GW::effects::g_post_process_effect_original = nullptr;
    GW::effects::g_drop_buff_func = nullptr;
    GW::effects::g_alcohol_level = 0;
}

}  // namespace

namespace GW::effects {

bool Initialize() {
    CrashContextScope context("startup", "effects", "initialize");
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
    CrashContextScope context("shutdown", "effects", "shutdown");
    if (!g_initialized) {
        return;
    }

    DisableHooks();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace GW::effects
