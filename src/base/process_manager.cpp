#include "base/error_handling.h"

#include "base/process_manager.h"

namespace py4gw::process_manager {

namespace {

HMODULE g_module_handle = nullptr;

}

void SetModuleHandle(HMODULE module) {
    g_module_handle = module;
}

HMODULE GetModuleHandle() {
    return g_module_handle;
}

std::filesystem::path GetModuleDirectory() {
    if (!g_module_handle) {
        return {};
    }

    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(g_module_handle, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path GetProcessDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path(buffer).parent_path();
}

}  // namespace py4gw::process_manager
