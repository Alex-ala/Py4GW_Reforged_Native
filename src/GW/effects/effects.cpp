#include "base/error_handling.h"

#include "GW/effects/effects.h"
#include "GW/effects/effects_methods.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

bool ResolvePostProcessEffect() {
    CrashContextScope context("startup", "effects", "resolve_post_process_effect");
    const auto* pattern = py4gw::Patterns::Get("effects.post_process_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.post_process_target", "effects");
        return false;
    }

    const uintptr_t address = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    gw::effects::g_post_process_effect_func = reinterpret_cast<gw::effects::PostProcessEffectFn>(address);
    return Logger::AssertAddress(
        "PostProcessEffect_Func",
        reinterpret_cast<uintptr_t>(gw::effects::g_post_process_effect_func),
        "effects");
}

bool ResolveDropBuff() {
    CrashContextScope context("startup", "effects", "resolve_drop_buff");
    const auto* pattern = py4gw::Patterns::Get("effects.drop_buff_callsite");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: effects.drop_buff_callsite", "effects");
        return false;
    }

    const uintptr_t callsite = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("DropBuff_Callsite", callsite, "effects")) {
        return false;
    }

    gw::effects::g_drop_buff_func = reinterpret_cast<gw::effects::DropBuffFn>(
        py4gw::Scanner::FunctionFromNearCall(callsite));
    return Logger::AssertAddress("DropBuff_Func", reinterpret_cast<uintptr_t>(gw::effects::g_drop_buff_func), "effects");
}

void __cdecl OnPostProcessEffect(uint32_t intensity, uint32_t tint) {
    py4gw::HookBase::EnterHook();
    gw::effects::g_alcohol_level = intensity;

    if (gw::effects::g_post_process_effect_original) {
        gw::effects::g_post_process_effect_original(intensity, tint);
    }

    py4gw::HookBase::LeaveHook();
}

bool Init() {
    CrashContextScope context("startup", "effects", "init");

    if (!ResolvePostProcessEffect() || !ResolveDropBuff()) {
        return false;
    }

    const int status = py4gw::HookBase::CreateHook(
        reinterpret_cast<void**>(&gw::effects::g_post_process_effect_func),
        reinterpret_cast<void*>(&OnPostProcessEffect),
        reinterpret_cast<void**>(&gw::effects::g_post_process_effect_original));
    return Logger::AssertHook("PostProcessEffect_Func", status, "effects");
}

void EnableHooks() {
    CrashContextScope context("runtime", "effects", "enable_hooks");
    if (gw::effects::g_post_process_effect_func) {
        py4gw::HookBase::EnableHooks(reinterpret_cast<void*>(gw::effects::g_post_process_effect_func));
    }
}

void DisableHooks() {
    CrashContextScope context("shutdown", "effects", "disable_hooks");
    if (gw::effects::g_post_process_effect_func) {
        py4gw::HookBase::DisableHooks(reinterpret_cast<void*>(gw::effects::g_post_process_effect_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "effects", "exit");
    if (gw::effects::g_post_process_effect_func) {
        py4gw::HookBase::RemoveHook(reinterpret_cast<void*>(gw::effects::g_post_process_effect_func));
    }

    gw::effects::g_post_process_effect_func = nullptr;
    gw::effects::g_post_process_effect_original = nullptr;
    gw::effects::g_drop_buff_func = nullptr;
    gw::effects::g_alcohol_level = 0;
}

}  // namespace

namespace gw::effects {

bool Initialize() {
    CrashContextScope context("startup", "effects", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(py4gw::Scanner::Initialize());
    PY4GW_ASSERT(py4gw::Patterns::Initialize());

    py4gw::HookBase::Initialize();
    if (!Init()) {
        Exit();
        py4gw::HookBase::Deinitialize();
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
    py4gw::HookBase::Deinitialize();
    g_initialized = false;
}

}  // namespace gw::effects
