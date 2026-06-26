#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace py4gw {

enum class ScannerSection : uint8_t {
    Text = 0,
    RData = 1,
    Data = 2,
    Count = 3,
};

struct ScannerSectionRange {
    uintptr_t start = 0;
    uintptr_t end = 0;
};

class Scanner {
public:
    static bool Initialize(HMODULE module = nullptr);
    static uintptr_t Find(const char* pattern, const char* mask, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindAssertion(const char* assertion_file, const char* assertion_message, uint32_t line_number = 0, int offset = 0);
    static uintptr_t FindInRange(const char* pattern, const char* mask, int offset, uintptr_t start, uintptr_t end);
    static uintptr_t ToFunctionStart(uintptr_t address, uint32_t scan_range = 0xFF);
    static uintptr_t FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr = true);
    static uintptr_t FindUseOfAddress(uintptr_t address, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindNthUseOfAddress(uintptr_t address, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindUseOfString(const char* value, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindUseOfString(const wchar_t* value, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindNthUseOfString(const char* value, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Text);
    static uintptr_t FindNthUseOfString(const wchar_t* value, size_t nth, int offset = 0, ScannerSection section = ScannerSection::Text);
    static bool IsValidPtr(uintptr_t address, ScannerSection section = ScannerSection::Data);
    static ScannerSectionRange GetSectionRange(ScannerSection section);
};

}  // namespace py4gw
