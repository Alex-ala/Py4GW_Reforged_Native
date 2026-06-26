#pragma once

#include "base/error_handling.h"

#include "base/scanner.h"

#include <Windows.h>
#include <cstdint>

namespace py4gw {

class FileScanner {
public:
    static bool CreateFromPath(const wchar_t* path, FileScanner* result);

    FileScanner();
    FileScanner(const void* file_mapping, uintptr_t image_base, PIMAGE_SECTION_HEADER sections, size_t count);
    ~FileScanner();

    void GetSectionAddressRange(ScannerSection section, uintptr_t* start, uintptr_t* end);
    uintptr_t FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset);
    uintptr_t FindInRange(const char* pattern, const char* mask, int offset, uint32_t start, uint32_t end);
    uintptr_t Find(const char* pattern, const char* mask, int offset, ScannerSection section = ScannerSection::Text);

    ScannerSectionRange sections[static_cast<size_t>(ScannerSection::Count)] = {};

private:
    const void* FileMapping = nullptr;
    uintptr_t ImageBase = 0;
};

}  // namespace py4gw
