#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace PY4GW {

class MemoryManager {
public:
    static bool Scan();

    static uint32_t GetGWVersion();
    static DWORD GetSkillTimer();
    static HWND GetGWWindowHandle();

    static void* MemAlloc(size_t size);
    static void* MemRealloc(void* buffer, size_t new_size);
    static void MemFree(void* buffer);
};

}  // namespace PY4GW
