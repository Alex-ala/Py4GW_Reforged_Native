#pragma once

#include "base/error_handling.h"

namespace py4gw::imgui::demo_ui {

bool Initialize();
void Shutdown();
void BeginFrame();
void Render(bool* request_shutdown);

}  // namespace py4gw::imgui::demo_ui
