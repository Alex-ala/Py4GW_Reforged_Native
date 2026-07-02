#pragma once

#include "base/error_handling.h"

#include <cstddef>

namespace PY4GW::imgui::addons::markdown_demo {

void Render();

// Bridge for Python bindings. imgui_markdown.h has non-inline functions and can
// only be included in markdown_demo.cpp, so rendering is exposed through here.
void RenderText(const char* text, std::size_t len);

}  // namespace PY4GW::imgui::addons::markdown_demo
