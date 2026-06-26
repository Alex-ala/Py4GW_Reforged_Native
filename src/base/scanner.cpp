#include "base/error_handling.h"

#include "base/scanner.h"
#include "base/file_scanner.h"
#include "base/logger.h"
#include <DbgHelp.h>

#include <cstring>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace {

py4gw::FileScanner fileScanner;

uintptr_t section_offset_from_disk = 0;
py4gw::ScannerSectionRange mem_sections[static_cast<size_t>(py4gw::ScannerSection::Count)] = {};

}

namespace py4gw {

bool Scanner::Initialize(HMODULE hModule) {
    if (!hModule) {
        hModule = GetModuleHandleW(nullptr);
        if (!hModule) {
            Logger::Instance().LogError("GetModuleHandleW failed.");
            return false;
        }
    }

    wchar_t filename[255];
    if (!(hModule && GetModuleFileNameW(hModule, filename, sizeof(filename) / sizeof(filename[0])))) {
        Logger::Instance().LogError("GetModuleFileNameW failed.");
        return false;
    }
    if (!FileScanner::CreateFromPath(filename, &fileScanner)) {
        Logger::Instance().LogError("FileScanner::CreateFromPath failed.");
        return false;
    }
    uint32_t dllImageBase = (uint32_t)hModule;
    IMAGE_NT_HEADERS* pNtHdr = ImageNtHeader(hModule);
    if (!pNtHdr) {
        Logger::Instance().LogError("ImageNtHeader failed.");
        return false;
    }
    IMAGE_SECTION_HEADER* pSectionHdr = (IMAGE_SECTION_HEADER*)(pNtHdr + 1);
    for (int i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++)
    {
        char* name = (char*)pSectionHdr->Name;
        uint8_t section = 0x8;
        if (memcmp(name, ".text", 5) == 0)
            section = static_cast<uint8_t>(ScannerSection::Text);
        else if (memcmp(name, ".rdata", 6) == 0)
            section = static_cast<uint8_t>(ScannerSection::RData);
        else if (memcmp(name, ".data", 5) == 0)
            section = static_cast<uint8_t>(ScannerSection::Data);
        if (section != 0x8) {
            mem_sections[section].start = dllImageBase + pSectionHdr->VirtualAddress;
            mem_sections[section].end = mem_sections[section].start + pSectionHdr->Misc.VirtualSize;
        }
        pSectionHdr++;
    }
    if (!(mem_sections[static_cast<size_t>(ScannerSection::Text)].start && mem_sections[static_cast<size_t>(ScannerSection::Text)].end)) {
        Logger::Instance().LogError("Scanner text section not found.");
        return false;
    }

    section_offset_from_disk =
        fileScanner.sections[static_cast<size_t>(ScannerSection::Text)].start -
        mem_sections[static_cast<size_t>(ScannerSection::Text)].start;
    return true;
}

uintptr_t Scanner::FindAssertion(const char* assertion_file, const char* assertion_msg, uint32_t line_number, int offset) {
    const auto found = fileScanner.FindAssertion(assertion_file, assertion_msg, line_number, offset);
    return found ? found - section_offset_from_disk : found;
}

uintptr_t Scanner::FindInRange(const char* pattern, const char* mask, int offset, uintptr_t start, uintptr_t end) {
    const auto found = fileScanner.FindInRange(pattern, mask, offset, (uint32_t)(start + section_offset_from_disk), (uint32_t)(end + section_offset_from_disk));
    return found ? found - section_offset_from_disk : found;
}

ScannerSectionRange Scanner::GetSectionRange(ScannerSection section) {
    ScannerSectionRange range = {};
    fileScanner.GetSectionAddressRange(section, &range.start, &range.end);
    return range;
}

uintptr_t Scanner::Find(const char* pattern, const char* mask, int offset, ScannerSection section) {
    return FindInRange(pattern, mask, offset, mem_sections[static_cast<size_t>(section)].start, mem_sections[static_cast<size_t>(section)].end);
}

bool Scanner::IsValidPtr(uintptr_t address, ScannerSection section) {
    return address &&
        address > mem_sections[static_cast<size_t>(section)].start &&
        address < mem_sections[static_cast<size_t>(section)].end;
}

uintptr_t Scanner::FunctionFromNearCall(uintptr_t call_instruction_address, bool check_valid_ptr) {
    if (!IsValidPtr(call_instruction_address, ScannerSection::Text)) {
        return 0;
    }
    uintptr_t function_address = 0;
    switch (((*(uintptr_t*)call_instruction_address) & 0x000000ff)) {
    case 0xe8:
    case 0xe9: {
        const auto near_address = *(uintptr_t*)(call_instruction_address + 1);
        function_address = (near_address)+(call_instruction_address + 5);
    } break;
    case 0xeb: {
        const auto near_address = *(char*)(call_instruction_address + 1);
        function_address = (near_address)+(call_instruction_address + 2);
    } break;
    default:
        return 0;
    }
    if (check_valid_ptr && !IsValidPtr(function_address, ScannerSection::Text)) {
        return 0;
    }
    if (const auto nested_call = FunctionFromNearCall(function_address, check_valid_ptr)) {
        return nested_call;
    }
    return function_address;
}

uintptr_t Scanner::FindUseOfAddress(uintptr_t address, int offset, ScannerSection section) {
    return FindNthUseOfAddress(address, 0, offset, section);
}

uintptr_t Scanner::FindNthUseOfAddress(uintptr_t address, size_t nth, int offset, ScannerSection section) {
    if (!address) {
        return 0;
    }
    const uintptr_t file_address = address;

    char pattern[4];
    memcpy(pattern, &file_address, sizeof(pattern));
    const char* mask = "xxxx";

    uintptr_t start = 0, end = 0;
    ScannerSectionRange range = GetSectionRange(section);
    start = range.start - section_offset_from_disk;
    end = range.end - section_offset_from_disk;

    auto found = FindInRange(pattern, mask, 0, start, end);
    if (!found) {
        return 0;
    }

    for (size_t i = 0; i < nth; i++) {
        found = FindInRange(pattern, mask, 0, found + 1, end);
        if (!found) {
            return 0;
        }
    }

    return found + offset;
}

uintptr_t Scanner::FindNthUseOfString(const wchar_t* str, size_t nth, int offset, ScannerSection section) {
    const size_t str_len = wcslen(str);
    std::string mask((str_len * 2) + 1, 'x');
    const auto found_str = Find((const char*)str, mask.c_str(), 0, ScannerSection::RData);
    if (!found_str) {
        return 0;
    }
    const auto first_null_char = FindInRange("\x0\x0", "xx", 2, found_str, found_str - 0x128);
    if (!first_null_char) {
        return 0;
    }
    return FindNthUseOfAddress(first_null_char, nth, offset, section);
}

uintptr_t Scanner::FindNthUseOfString(const char* str, size_t nth, int offset, ScannerSection section) {
    const size_t str_len = strlen(str);
    std::string mask(str_len + 1, 'x');
    const auto found_str = Find(str, mask.c_str(), 0, ScannerSection::RData);
    if (!found_str) {
        return 0;
    }
    const auto first_null_char = FindInRange("\x0", "x", 1, found_str, found_str - 0x64);
    if (!first_null_char) {
        return 0;
    }
    return FindNthUseOfAddress(first_null_char, nth, offset, section);
}

uintptr_t Scanner::FindUseOfString(const char* str, int offset, ScannerSection section) {
    return FindNthUseOfString(str, 0, offset, section);
}

uintptr_t Scanner::FindUseOfString(const wchar_t* str, int offset, ScannerSection section) {
    return FindNthUseOfString(str, 0, offset, section);
}

uintptr_t Scanner::ToFunctionStart(uintptr_t call_instruction_address, uint32_t scan_range) {
    if (!call_instruction_address) {
        return 0;
    }
    return FindInRange("\x55\x8b\xec", "xxx", 0, call_instruction_address, call_instruction_address - scan_range);
}

}  // namespace py4gw
