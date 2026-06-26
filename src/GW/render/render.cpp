#include "base/error_handling.h"

#include "GW/render/render.h"
#include "GW/render/render_methods.h"

#include "base/panic.h"
#include "base/hooker.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "base/logger.h"

namespace {

bool ResolveWindowHandlePointer() {
    const auto* pattern = py4gw::Patterns::Get("render.window_handle_ptr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.window_handle_ptr", "render");
        return false;
    }

    const uintptr_t scan = py4gw::Scanner::Find(
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
    if (!py4gw::Scanner::IsValidPtr(candidate, py4gw::ScannerSection::Data)) {
        Logger::Instance().LogError("window_handle_ptr is outside the expected data section.", "render");
        return false;
    }

    gw::render::g_window_handle_ptr = candidate;
    return true;
}

bool ResolveResetHook() {
    const auto* pattern = py4gw::Patterns::Get("render.reset_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.reset_target", "render");
        return false;
    }

    const uintptr_t scan = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        0,
        pattern->section);
    if (!Logger::AssertAddress("GwReset_Scan", scan, "render")) {
        return false;
    }

    gw::render::g_reset_func = reinterpret_cast<gw::render::ResetFn>(
        py4gw::Scanner::ToFunctionStart(scan, static_cast<uint32_t>(pattern->offset)));
    return Logger::AssertAddress("GwReset_Func", reinterpret_cast<uintptr_t>(gw::render::g_reset_func), "render");
}

bool ResolveEndSceneHook() {
    const auto* pattern = py4gw::Patterns::Get("render.end_scene_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: render.end_scene_target", "render");
        return false;
    }

    const uintptr_t scan = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("GwEndScene_Scan", scan, "render")) {
        return false;
    }

    gw::render::g_end_scene_func =
        reinterpret_cast<gw::render::EndSceneFn>(py4gw::Scanner::ToFunctionStart(scan));
    return Logger::AssertAddress("GwEndScene_Func", reinterpret_cast<uintptr_t>(gw::render::g_end_scene_func), "render");
}

bool ResolveGetTransformFunction() {
    // Forward parity note: original RenderMgr resolved this directly from a literal pattern.
    // Keep that behavior visible until these patterns are moved into JSON with the rest.
    gw::render::g_get_transform_func = reinterpret_cast<gw::render::GetTransformFn>(
        py4gw::Scanner::ToFunctionStart(py4gw::Scanner::Find("\x7C\x14\x68\xDB\x02\x00\x00", "xxxxxxx")));
    return Logger::AssertAddress(
        "GwGetTransform_func",
        reinterpret_cast<uintptr_t>(gw::render::g_get_transform_func),
        "render");
}

bool __cdecl OnEndScene(gw::render::GwDxContext* ctx, void* unk) {
    py4gw::HookBase::EnterHook();
    ::EnterCriticalSection(&gw::render::g_render_lock);
    gw::render::g_in_render_loop = true;
    gw::render::g_dx_context = ctx;
    if (gw::render::g_render_callback) {
        gw::render::g_render_callback(ctx->device);
    }
    const bool retval = gw::render::g_end_scene_original(ctx, unk);
    gw::render::g_in_render_loop = false;
    ::LeaveCriticalSection(&gw::render::g_render_lock);
    py4gw::HookBase::LeaveHook();
    return retval;
}

/*
bool ResolveScreenCaptureHook() {
    // Original RenderMgr logic:
    // ScreenCapture_Func = (GwEndScene_pt)Scanner::ToFunctionStart(
    //     Scanner::FindAssertion("Dx9Dev.cpp", "No valid case for switch variable 'mode.Format'", 0, 0), 0xfff);
    // Logger::AssertAddress("ScreenCapture_Func", (uintptr_t)ScreenCapture_Func, "RenderModule");
}

bool __cdecl OnScreenCapture(gw::render::GwDxContext* ctx, void* unk) {
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

bool __cdecl OnReset(gw::render::GwDxContext* ctx) {
    py4gw::HookBase::EnterHook();
    gw::render::g_dx_context = ctx;
    if (gw::render::g_reset_callback) {
        gw::render::g_reset_callback(ctx->device);
    }
    const bool retval = gw::render::g_reset_original(ctx);
    py4gw::HookBase::LeaveHook();
    return retval;
}

bool Init() {
    ::InitializeCriticalSection(&gw::render::g_render_lock);
    gw::render::g_render_lock_initialized = true;

    if (!ResolveWindowHandlePointer() ||
        !ResolveResetHook() ||
        !ResolveEndSceneHook() ||
        !ResolveGetTransformFunction()) {
        return false;
    }

    const int end_scene_status = py4gw::HookBase::CreateHook(
        reinterpret_cast<void**>(&gw::render::g_end_scene_func),
        reinterpret_cast<void*>(&OnEndScene),
        reinterpret_cast<void**>(&gw::render::g_end_scene_original));
    const int reset_status = py4gw::HookBase::CreateHook(
        reinterpret_cast<void**>(&gw::render::g_reset_func),
        reinterpret_cast<void*>(&OnReset),
        reinterpret_cast<void**>(&gw::render::g_reset_original));

    const bool end_scene_ok = Logger::AssertHook("GwEndScene_Func", end_scene_status, "render");
    const bool reset_ok = Logger::AssertHook("GwReset_Func", reset_status, "render");
    return end_scene_ok && reset_ok;
}

void EnableHooks() {
    if (gw::render::g_end_scene_func) {
        py4gw::HookBase::EnableHooks(reinterpret_cast<void*>(gw::render::g_end_scene_func));
    }
    if (gw::render::g_reset_func) {
        py4gw::HookBase::EnableHooks(reinterpret_cast<void*>(gw::render::g_reset_func));
    }
    gw::render::g_hooks_enabled = true;
}

void DisableHooks() {
    if (!gw::render::g_hooks_enabled) {
        return;
    }
    if (gw::render::g_end_scene_func) {
        py4gw::HookBase::DisableHooks(reinterpret_cast<void*>(gw::render::g_end_scene_func));
    }
    if (gw::render::g_reset_func) {
        py4gw::HookBase::DisableHooks(reinterpret_cast<void*>(gw::render::g_reset_func));
    }
    gw::render::g_hooks_enabled = false;
}

void Exit() {
    if (gw::render::g_end_scene_func) {
        py4gw::HookBase::RemoveHook(reinterpret_cast<void*>(gw::render::g_end_scene_func));
    }
    if (gw::render::g_reset_func) {
        py4gw::HookBase::RemoveHook(reinterpret_cast<void*>(gw::render::g_reset_func));
    }
    if (gw::render::g_render_lock_initialized) {
        ::DeleteCriticalSection(&gw::render::g_render_lock);
        gw::render::g_render_lock_initialized = false;
    }

    gw::render::g_dx_context = nullptr;
    gw::render::g_window_handle_ptr = 0;
    gw::render::g_end_scene_func = nullptr;
    gw::render::g_end_scene_original = nullptr;
    gw::render::g_reset_func = nullptr;
    gw::render::g_reset_original = nullptr;
    gw::render::g_get_transform_func = nullptr;
    gw::render::g_hooks_enabled = false;
}

}  // namespace

namespace gw::render {

bool Initialize() {
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
    if (!g_initialized) {
        return;
    }

    DisableHooks();

    for (int i = 0; i < 10; ++i) {
        if (py4gw::HookBase::GetInHookCount() == 0) {
            break;
        }
        ::Sleep(16);
    }

    Exit();
    py4gw::HookBase::Deinitialize();

    g_render_callback = nullptr;
    g_reset_callback = nullptr;
    g_in_render_loop = false;
    g_initialized = false;
}

}  // namespace gw::render
