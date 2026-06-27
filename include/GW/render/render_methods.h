#pragma once

#include "base/error_handling.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

struct IDirect3DDevice9;

namespace gw::render {

using RenderCallback = void(__cdecl*)(IDirect3DDevice9*);

struct Mat4x3f {
    float _11;
    float _12;
    float _13;
    float _14;
    float _21;
    float _22;
    float _23;
    float _24;
    float _31;
    float _32;
    float _33;
    float _34;

    enum Flags {
        Shear = 1 << 3
    };

    uint32_t flags;
};

enum class Transform : int {
    ProjectionMatrix = 0,
    ModelMatrix = 1,
    Count = 5,
};

HWND GetWindowHandle();
IDirect3DDevice9* GetDevice();
bool GetIsInRenderLoop();
int GetIsFullscreen();
uint32_t GetViewportWidth();
uint32_t GetViewportHeight();
Mat4x3f* GetTransform(Transform transform);
float GetFieldOfView();

RenderCallback GetRenderCallback();
void SetRenderCallback(RenderCallback callback);
void SetResetCallback(RenderCallback callback);

struct GwDxContext {
    uint8_t h0000_1[0x128];
    uint8_t h0000[24];
    uint32_t h0018;
    uint8_t h001C[44];
    wchar_t gpu_name[32];
    uint8_t h0088[8];
    IDirect3DDevice9* device;
    uint8_t h0094[12];
    uint32_t framecount;
    uint8_t h00A4[2936];
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint8_t h0C24[148];
    uint32_t window_width;
    uint32_t window_height;
    uint8_t h0CC0[952];
};

using EndSceneFn = bool(__cdecl*)(GwDxContext* ctx, void* unk);
using ResetFn = bool(__cdecl*)(GwDxContext* ctx);
using GetTransformFn = Mat4x3f*(__cdecl*)(int transform);

extern GwDxContext* g_dx_context;
extern uintptr_t g_window_handle_ptr;
extern EndSceneFn g_end_scene_func;
extern EndSceneFn g_end_scene_original;
extern ResetFn g_reset_func;
extern ResetFn g_reset_original;
extern GetTransformFn g_get_transform_func;

extern CRITICAL_SECTION g_render_lock;
extern std::atomic<int> g_active_render_hooks;
extern std::atomic<bool> g_in_render_loop;
extern bool g_render_lock_initialized;
extern bool g_hooks_enabled;
extern std::atomic<bool> g_initialized;
extern std::atomic<bool> g_shutting_down;

extern RenderCallback g_render_callback;
extern RenderCallback g_reset_callback;

}  // namespace gw::render
