#pragma once

#include "base/error_handling.h"

#include <string>
#include <windows.h>

class CrashHandler {
public:
    static CrashHandler& Instance();

    void Initialize();
    void Terminate();

    bool OnException(EXCEPTION_POINTERS* info, const char* source, bool recoverable);
    std::string CrashDirUtf8() const;

private:
    CrashHandler() = default;
    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    static LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* info);
    static void OnPanic(void* context, const char* expr, const char* message,
                        const char* file, unsigned int line, const char* function);
    static uintptr_t __cdecl AppendStackDetour(void* debug_info, uint32_t a2, uint32_t a3,
                                               uint32_t a4, CONTEXT* ctx, uint32_t a6,
                                               uint32_t a7);

    void InstallPathA();
    void InstallPathC();
    void ClearCallbackFilterPolicy();
    void RestoreCallbackFilterPolicy();
    bool EnsureCrashDir();
    void WriteSidecar(EXCEPTION_POINTERS* info, const wchar_t* json_path,
                      const wchar_t* dmp_name, const wchar_t* gwtext_name, const char* source);
    void WriteDump(EXCEPTION_POINTERS* info, const wchar_t* dmp_path, const char* comment);
};
