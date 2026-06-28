#include "base/error_handling.h"

#include "GW/render/render.h"

#include "base/panic.h"
#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "base/logger.h"

namespace {

bool WaitForRenderHooksToDrain() {
    CrashContextScope context("shutdown", "render", "wait_for_hooks_to_drain");
    for (int i = 0; i < 125; ++i) {
        if (GW::render::g_active_render_hooks.load() == 0 &&
            !GW::render::g_in_render_loop.load()) {
            return true;
        }
        ::Sleep(16);
    }

    Logger::Instance().LogWarning("[render] Timed out waiting for in-flight render hooks to drain.", "render");
    return false;
}

bool ResolveWindowHandlePointer() {
    CrashContextScope context("startup", "render", "resolve_window_handle_ptr");
    const auto* pattern = PY4GW::Patterns::Get("render.window_handle_ptr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.window_handle_ptr", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("window_handle_ptr_scan", scan, "render")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(scan);
    if (!Logger::AssertAddress("window_handle_ptr", candidate, "render")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate, PY4GW::ScannerSection::Data)) {
        Logger::Instance().LogError("window_handle_ptr is outside the expected data section.", "render");
        return false;
    }

    GW::render::g_window_handle_ptr = candidate;
    return true;
}

bool ResolveResetHook() {
    CrashContextScope context("startup", "render", "resolve_reset_hook");
    const auto* pattern = PY4GW::Patterns::Get("render.reset_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.reset_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        0,
        pattern->section);
    if (!Logger::AssertAddress("GwReset_Scan", scan, "render")) {
        return false;
    }

    GW::render::g_reset_func = reinterpret_cast<GW::render::ResetFn>(
        PY4GW::Scanner::ToFunctionStart(scan, static_cast<uint32_t>(pattern->offset)));
    return Logger::AssertAddress("GwReset_Func", reinterpret_cast<uintptr_t>(GW::render::g_reset_func), "render");
}

bool ResolveEndSceneHook() {
    CrashContextScope context("startup", "render", "resolve_end_scene_hook");
    const auto* pattern = PY4GW::Patterns::Get("render.end_scene_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.end_scene_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("GwEndScene_Scan", scan, "render")) {
        return false;
    }

    GW::render::g_end_scene_func =
        reinterpret_cast<GW::render::EndSceneFn>(PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress("GwEndScene_Func", reinterpret_cast<uintptr_t>(GW::render::g_end_scene_func), "render");
}

bool ResolveGetTransformFunction() {
    CrashContextScope context("startup", "render", "resolve_get_transform");
    const auto* pattern = PY4GW::Patterns::Get("render.get_transform_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.get_transform_target", "render");
        return false;
    }

    const uintptr_t scan = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("GwGetTransform_scan", scan, "render")) {
        return false;
    }

    GW::render::g_get_transform_func =
        reinterpret_cast<GW::render::GetTransformFn>(PY4GW::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress(
        "GwGetTransform_func",
        reinterpret_cast<uintptr_t>(GW::render::g_get_transform_func),
        "render");
}

bool __cdecl OnEndScene(GW::render::GwDxContext* ctx, void* unk) {
    PY4GW::HookBase::EnterHook();
    ++GW::render::g_active_render_hooks;

    if (GW::render::g_shutting_down || !GW::render::g_render_lock_initialized) {
        const bool retval = GW::render::g_end_scene_original ? GW::render::g_end_scene_original(ctx, unk) : false;
        --GW::render::g_active_render_hooks;
        PY4GW::HookBase::LeaveHook();
        return retval;
    }

    ::EnterCriticalSection(&GW::render::g_render_lock);
    GW::render::g_in_render_loop = true;
    GW::render::g_dx_context = ctx;
    if (!GW::render::g_shutting_down && GW::render::g_render_callback) {
        GW::render::g_render_callback(ctx->device);
    }
    const bool retval = GW::render::g_end_scene_original(ctx, unk);
    GW::render::g_in_render_loop = false;
    ::LeaveCriticalSection(&GW::render::g_render_lock);
    --GW::render::g_active_render_hooks;
    PY4GW::HookBase::LeaveHook();
    return retval;
}

/*
bool ResolveScreenCaptureHook() {
    // Original RenderMgr logic:
    // ScreenCapture_Func = (GwEndScene_pt)Scanner::ToFunctionStart(
    //     Scanner::FindAssertion("Dx9Dev.cpp", "No valid case for switch variable 'mode.Format'", 0, 0), 0xfff);
    // Logger::AssertAddress("ScreenCapture_Func", (uintptr_t)ScreenCapture_Func, "RenderModule");
}

bool __cdecl OnScreenCapture(GW::render::GwDxContext* ctx, void* unk) {
    // Original RenderMgr logic:
    // HookBase::EnterHook();
    // if (!GW::UI::GetIsShiftScreenShot() && render_callback) {
    //     render_callback(ctx->device);
    // }
    // bool retval = RetScreenCapture(ctx, unk);
    // HookBase::LeaveHook();
    // return retval;
}

Forward parity note:
- Screen capture is intentionally left commented out until the required UI screenshot-state dependency is migrated.
- Keeping the original logic here is preferable to activating a partial hook path.
*/

bool __cdecl OnReset(GW::render::GwDxContext* ctx) {
    PY4GW::HookBase::EnterHook();
    ++GW::render::g_active_render_hooks;
    GW::render::g_dx_context = ctx;
    if (!GW::render::g_shutting_down && GW::render::g_reset_callback) {
        GW::render::g_reset_callback(ctx->device);
    }
    const bool retval = GW::render::g_reset_original ? GW::render::g_reset_original(ctx) : false;
    --GW::render::g_active_render_hooks;
    PY4GW::HookBase::LeaveHook();
    return retval;
}

bool Init() {
    CrashContextScope context("startup", "render", "init");
    GW::render::g_shutting_down = false;
    ::InitializeCriticalSection(&GW::render::g_render_lock);
    GW::render::g_render_lock_initialized = true;

    if (!ResolveWindowHandlePointer() ||
        !ResolveResetHook() ||
        !ResolveEndSceneHook() ||
        !ResolveGetTransformFunction()) {
        return false;
    }

    const int end_scene_status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&GW::render::g_end_scene_func),
        reinterpret_cast<void*>(&OnEndScene),
        reinterpret_cast<void**>(&GW::render::g_end_scene_original));
    const int reset_status = PY4GW::HookBase::CreateHook(
        reinterpret_cast<void**>(&GW::render::g_reset_func),
        reinterpret_cast<void*>(&OnReset),
        reinterpret_cast<void**>(&GW::render::g_reset_original));

    const bool end_scene_ok = Logger::AssertHook("GwEndScene_Func", end_scene_status, "render");
    const bool reset_ok = Logger::AssertHook("GwReset_Func", reset_status, "render");
    return end_scene_ok && reset_ok;
}

void EnableHooks() {
    CrashContextScope context("runtime", "render", "enable_hooks");
    if (GW::render::g_end_scene_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(GW::render::g_end_scene_func));
    }
    if (GW::render::g_reset_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(GW::render::g_reset_func));
    }
    GW::render::g_hooks_enabled = true;
}

void DisableHooks() {
    CrashContextScope context("shutdown", "render", "disable_hooks");
    if (!GW::render::g_hooks_enabled) {
        return;
    }
    if (GW::render::g_end_scene_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(GW::render::g_end_scene_func));
    }
    if (GW::render::g_reset_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(GW::render::g_reset_func));
    }
    GW::render::g_hooks_enabled = false;
}

void Exit() {
    CrashContextScope context("shutdown", "render", "exit");
    GW::render::g_render_callback = nullptr;
    GW::render::g_reset_callback = nullptr;
    GW::render::g_in_render_loop = false;
    if (GW::render::g_end_scene_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(GW::render::g_end_scene_func));
    }
    if (GW::render::g_reset_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(GW::render::g_reset_func));
    }
    if (GW::render::g_render_lock_initialized) {
        ::DeleteCriticalSection(&GW::render::g_render_lock);
        GW::render::g_render_lock_initialized = false;
    }

    GW::render::g_dx_context = nullptr;
    GW::render::g_window_handle_ptr = 0;
    GW::render::g_end_scene_func = nullptr;
    GW::render::g_end_scene_original = nullptr;
    GW::render::g_reset_func = nullptr;
    GW::render::g_reset_original = nullptr;
    GW::render::g_get_transform_func = nullptr;
    GW::render::g_hooks_enabled = false;
    GW::render::g_active_render_hooks = 0;
}

}  // namespace

namespace GW::render {

bool Initialize() {
    CrashContextScope context("startup", "render", "initialize");
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
    CrashContextScope context("shutdown", "render", "shutdown");
    if (!g_initialized) {
        return;
    }

    g_shutting_down = true;
    g_render_callback = nullptr;
    g_reset_callback = nullptr;
    DisableHooks();
    WaitForRenderHooksToDrain();

    Exit();
    PY4GW::HookBase::Deinitialize();

    g_in_render_loop = false;
    g_shutting_down = false;
    g_initialized = false;
}

}  // namespace GW::render
