#pragma once

#include "base/error_handling.h"

#include <windows.h>

struct IDirect3DDevice9;

namespace py4gw::imgui {

using ShutdownCallback = void(*)();

class FontManager;

bool Initialize(IDirect3DDevice9* device);
void Shutdown();
bool BeginFrame(IDirect3DDevice9* device);
bool RenderConsoleUi(bool* request_shutdown);
void EndFrame(IDirect3DDevice9* device);
void InvalidateDeviceObjects();
void SetShutdownCallback(ShutdownCallback callback);

}  // namespace py4gw::imgui
