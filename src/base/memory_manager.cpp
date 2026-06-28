#include "base/memory_manager.h"

#include "base/error_handling.h"

#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <mmsystem.h>

namespace {

using GetGWVersionFn = uint32_t(__cdecl*)();
using MemAllocHelperFn = void*(__stdcall*)(size_t, uint8_t, const char*, int);
using MemReallocHelperFn = void*(__stdcall*)(void*, size_t, uint8_t, const char*, int);
using MemFreeFn = void*(__cdecl*)(void*);

DWORD* g_skill_timer_ptr = nullptr;
uintptr_t g_window_handle_ptr = 0;
uintptr_t g_get_personal_dir_ptr = 0;
GetGWVersionFn g_get_gw_version_func = nullptr;
MemAllocHelperFn g_mem_alloc_helper_func = nullptr;
MemReallocHelperFn g_mem_realloc_helper_func = nullptr;
MemFreeFn g_mem_free_func = nullptr;

bool ResolveSkillTimer() {
    const auto* pattern = PY4GW::Patterns::Get("memory.skill_timer_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: memory.skill_timer_anchor", "memory");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    address = PY4GW::Scanner::FunctionFromNearCall(address);
    if (!Logger::AssertAddress("SkillTimer_Func", address, "memory")) {
        return false;
    }

    address += 0x2;
    if (!Logger::AssertAddress("SkillTimer_PtrRef", address, "memory")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address))) {
        Logger::Instance().LogError("Skill timer pointer is outside the expected data section.", "memory");
        return false;
    }

    g_skill_timer_ptr = *reinterpret_cast<DWORD**>(address);
    return Logger::AssertAddress("SkillTimer_Ptr", reinterpret_cast<uintptr_t>(g_skill_timer_ptr), "memory");
}

bool ResolveWindowHandlePointer() {
    const auto* pattern = PY4GW::Patterns::Get("memory.window_handle_ptr");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: memory.window_handle_ptr", "memory");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("WinHandle_PtrRef", address, "memory")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<const uintptr_t*>(address))) {
        Logger::Instance().LogError("Window handle pointer is outside the expected data section.", "memory");
        return false;
    }

    g_window_handle_ptr = *reinterpret_cast<const uintptr_t*>(address);
    return Logger::AssertAddress("WinHandle_Ptr", g_window_handle_ptr, "memory");
}

bool ResolvePersonalDirFunction() {
    const auto* pattern = PY4GW::Patterns::Get("memory.personal_dir_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: memory.personal_dir_target", "memory");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    if (!Logger::AssertAddress("GetPersonalDir_Func", address, "memory")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(address, PY4GW::ScannerSection::Text)) {
        Logger::Instance().LogError("GetPersonalDir target is outside the expected text section.", "memory");
        return false;
    }

    g_get_personal_dir_ptr = address;
    return true;
}

bool ResolveVersionFunction() {
    const auto* pattern = PY4GW::Patterns::Get("memory.gw_version_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: memory.gw_version_anchor", "memory");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    address = PY4GW::Scanner::FunctionFromNearCall(address);
    g_get_gw_version_func = reinterpret_cast<GetGWVersionFn>(address);
    return Logger::AssertAddress("GetGWVersion_Func", reinterpret_cast<uintptr_t>(g_get_gw_version_func), "memory");
}

bool ResolveAllocHelpers() {
    const auto* alloc_pattern = PY4GW::Patterns::Get("memory.mem_alloc_helper");
    const auto* realloc_pattern = PY4GW::Patterns::Get("memory.mem_realloc_helper");
    if (!alloc_pattern || !realloc_pattern) {
        Logger::Instance().LogError("Missing or invalid memory helper pattern.", "memory");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::Find(
        alloc_pattern->pattern.c_str(),
        alloc_pattern->mask.c_str(),
        alloc_pattern->offset,
        alloc_pattern->section);
    g_mem_alloc_helper_func = reinterpret_cast<MemAllocHelperFn>(PY4GW::Scanner::ToFunctionStart(address));

    address = PY4GW::Scanner::Find(
        realloc_pattern->pattern.c_str(),
        realloc_pattern->mask.c_str(),
        realloc_pattern->offset,
        realloc_pattern->section);
    g_mem_realloc_helper_func = reinterpret_cast<MemReallocHelperFn>(PY4GW::Scanner::ToFunctionStart(address));

    const bool alloc_ok = Logger::AssertAddress(
        "MemAllocHelper_Func",
        reinterpret_cast<uintptr_t>(g_mem_alloc_helper_func),
        "memory");
    const bool realloc_ok = Logger::AssertAddress(
        "MemReallocHelper_Func",
        reinterpret_cast<uintptr_t>(g_mem_realloc_helper_func),
        "memory");
    return alloc_ok && realloc_ok;
}

bool ResolveFreeHelper() {
    const auto* pattern = PY4GW::Patterns::Get("memory.mem_free_anchor");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: memory.mem_free_anchor", "memory");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        pattern->assertion_file.c_str(),
        pattern->assertion_message.c_str(),
        static_cast<uint32_t>(pattern->line_number),
        pattern->offset);
    address = PY4GW::Scanner::FunctionFromNearCall(address);
    if (!Logger::AssertAddress("MemFree_Binder_Func", address, "memory")) {
        return false;
    }

    address += 0x39;
    if (!Logger::AssertAddress("MemFree_FuncRef", address, "memory")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(*reinterpret_cast<uintptr_t*>(address), PY4GW::ScannerSection::Text)) {
        Logger::Instance().LogError("MemFree target is outside the expected text section.", "memory");
        return false;
    }

    g_mem_free_func = *reinterpret_cast<MemFreeFn*>(address);
    return Logger::AssertAddress("MemFree_Func", reinterpret_cast<uintptr_t>(g_mem_free_func), "memory");
}

}  // namespace

namespace PY4GW {

bool MemoryManager::Scan() {
    PY4GW_ASSERT(Scanner::Initialize());
    PY4GW_ASSERT(Patterns::Initialize());

    return ResolveSkillTimer() &&
        ResolveWindowHandlePointer() &&
        ResolvePersonalDirFunction() &&
        ResolveVersionFunction() &&
        ResolveAllocHelpers() &&
        ResolveFreeHelper();
}

uint32_t MemoryManager::GetGWVersion() {
    return g_get_gw_version_func ? g_get_gw_version_func() : 0;
}

DWORD MemoryManager::GetSkillTimer() {
    return g_skill_timer_ptr ? timeGetTime() + *g_skill_timer_ptr : timeGetTime();
}

HWND MemoryManager::GetGWWindowHandle() {
    return g_window_handle_ptr ? *reinterpret_cast<HWND*>(g_window_handle_ptr) : nullptr;
}

void* MemoryManager::MemAlloc(size_t size) {
    if (!g_mem_alloc_helper_func) {
        return nullptr;
    }
    return g_mem_alloc_helper_func(size, 0, __FILE__, __LINE__);
}

void* MemoryManager::MemRealloc(void* buffer, size_t new_size) {
    if (!g_mem_realloc_helper_func) {
        return nullptr;
    }
    return g_mem_realloc_helper_func(buffer, new_size, 0, __FILE__, __LINE__);
}

void MemoryManager::MemFree(void* buffer) {
    if (g_mem_free_func) {
        g_mem_free_func(buffer);
    }
}

}  // namespace PY4GW
