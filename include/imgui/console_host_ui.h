#pragma once

#include "base/error_handling.h"

namespace PY4GW::imgui::console_host_ui {

bool Initialize();
void Shutdown();
void BeginFrame();
void Render(bool* request_shutdown);

}  // namespace PY4GW::imgui::console_host_ui
