#pragma once

#include "base/error_handling.h"

#include <cstddef>

namespace PY4GW::imgui::addons::hotkey_demo {

void Render();

// Bridge for Python bindings. imHotKey.h needs legacy Win32 key-state shims that
// live in this TU, so the hotkey editor is exposed through here instead of
// including the header in the bindings TU.
//
// names/libs/keys are parallel arrays of length `count`. On return, keys[] holds
// the (possibly edited) chord bitmasks.
void EditKeys(const char** names, const char** libs, unsigned int* keys, int count, const char* popup_label);
void KeyLib(unsigned int keys, char* buffer, std::size_t buffer_size);

}  // namespace PY4GW::imgui::addons::hotkey_demo
