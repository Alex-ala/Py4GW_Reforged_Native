#pragma once

#include "base/error_handling.h"

#include <filesystem>
#include <windows.h>

namespace py4gw::process_manager {

void SetModuleHandle(HMODULE module);
HMODULE GetModuleHandle();
std::filesystem::path GetModuleDirectory();
std::filesystem::path GetProcessDirectory();

}  // namespace py4gw::process_manager
