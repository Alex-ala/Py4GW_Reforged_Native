#include "base/error_handling.h"

#include "GW/render/render_methods.h"

namespace gw::render {

GwDxContext* g_dx_context = nullptr;
uintptr_t g_window_handle_ptr = 0;
EndSceneFn g_end_scene_func = nullptr;
EndSceneFn g_end_scene_original = nullptr;
ResetFn g_reset_func = nullptr;
ResetFn g_reset_original = nullptr;
GetTransformFn g_get_transform_func = nullptr;

CRITICAL_SECTION g_render_lock;
std::atomic<bool> g_in_render_loop = false;
bool g_render_lock_initialized = false;
bool g_hooks_enabled = false;
std::atomic<bool> g_initialized = false;

RenderCallback g_render_callback = nullptr;
RenderCallback g_reset_callback = nullptr;

HWND GetWindowHandle() {
    return g_window_handle_ptr ? *reinterpret_cast<HWND*>(g_window_handle_ptr) : nullptr;
}

IDirect3DDevice9* GetDevice() {
    return g_dx_context ? g_dx_context->device : nullptr;
}

bool GetIsInRenderLoop() {
    if (!g_render_lock_initialized) {
        return false;
    }
    ::EnterCriticalSection(&g_render_lock);
    const bool ret = g_in_render_loop;
    ::LeaveCriticalSection(&g_render_lock);
    return ret;
}

int GetIsFullscreen() {
    if (!g_dx_context) {
        return -1;
    }
    return g_dx_context->viewport_height == g_dx_context->window_height &&
            g_dx_context->viewport_width == g_dx_context->window_width
        ? 1
        : 0;
}

uint32_t GetViewportWidth() {
    return g_dx_context ? g_dx_context->viewport_width : 0;
}

uint32_t GetViewportHeight() {
    return g_dx_context ? g_dx_context->viewport_height : 0;
}

Mat4x3f* GetTransform(Transform transform) {
    return g_get_transform_func ? g_get_transform_func(static_cast<int>(transform)) : nullptr;
}

/*
float GetFieldOfView()
{
    // Original RenderMgr implementation:
    // const Camera* cam = CameraMgr::GetCamera();
    // if (!cam) return 0.f;
    // constexpr float dividend = 2.f/3.f + 1.f;
    // return atan2(1.0f, dividend / tan(cam->GetFieldOfView() * 0.5f)) * 2.0f;
}

Forward parity note:
- This function is intentionally left commented out until CameraMgr/Camera are migrated.
- Keeping the original shape here is preferable to shipping a fake implementation that returns 0.
*/

RenderCallback GetRenderCallback() {
    return g_render_callback;
}

void SetRenderCallback(RenderCallback callback) {
    g_render_callback = callback;
}

void SetResetCallback(RenderCallback callback) {
    g_reset_callback = callback;
}

}  // namespace gw::render
