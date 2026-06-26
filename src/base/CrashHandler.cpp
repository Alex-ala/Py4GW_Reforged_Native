#include "base/error_handling.h"

#include "base/CrashHandler.h"

#include "base/panic.h"
#include "base/hooker.h"
#include "base/patterns.h"
#include "base/process_manager.h"
#include "base/scanner.h"
#include "base/logger.h"

#include <DbgHelp.h>
#include <cstdio>
#include <cstring>

namespace {

volatile LONG s_handling = 0;
bool s_installed = false;
LPTOP_LEVEL_EXCEPTION_FILTER s_prev_filter = nullptr;
uintptr_t s_append_stack_fn = 0;
void* s_append_stack_orig = nullptr;
DWORD s_prev_policy = 0;
bool s_policy_changed = false;

wchar_t s_crash_dir[MAX_PATH] = {0};
bool s_crash_dir_ready = false;
std::string s_crash_dir_utf8;

char s_gw_text[32768] = {0};
char s_panic_expr[512] = {0};
char s_panic_message[1024] = {0};
char s_panic_file[260] = {0};
char s_panic_function[128] = {0};
unsigned int s_panic_line = 0;

const char* ExceptionLabel(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access_violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack_overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal_instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "int_divide_by_zero";
    case EXCEPTION_PRIV_INSTRUCTION: return "priv_instruction";
    case EXCEPTION_IN_PAGE_ERROR: return "in_page_error";
    case 0xC0000409: return "stack_buffer_overrun";
    case 0xE06D7363: return "cpp_exception";
    case 0x80000003: return "breakpoint";
    default: return "exception";
    }
}

struct JBuf {
    char* p;
    char* const e;
};

void JAppend(JBuf& buffer, const char* fmt, ...) {
    if (buffer.p >= buffer.e) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    const int written = _vsnprintf_s(buffer.p, static_cast<size_t>(buffer.e - buffer.p), _TRUNCATE, fmt, args);
    va_end(args);
    buffer.p = written >= 0 ? buffer.p + written : buffer.e;
}

void JsonEscape(char* dst, size_t cap, const char* src) {
    if (cap == 0) {
        return;
    }
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 2 < cap; ++i) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (c == '\\' || c == '"') {
            dst[j++] = '\\';
            dst[j++] = static_cast<char>(c);
        } else if (c == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if (c >= 0x20) {
            dst[j++] = static_cast<char>(c);
        }
    }
    dst[j] = 0;
}

void MakeDirTree(const wchar_t* path) {
    wchar_t temp[MAX_PATH] = {};
    wcsncpy_s(temp, path, _TRUNCATE);
    for (wchar_t* p = temp + 3; *p; ++p) {
        if (*p == L'\\') {
            *p = 0;
            ::CreateDirectoryW(temp, nullptr);
            *p = L'\\';
        }
    }
    ::CreateDirectoryW(temp, nullptr);
}

void BuildStem(wchar_t* out, size_t cap) {
    SYSTEMTIME time = {};
    ::GetLocalTime(&time);
    _snwprintf_s(
        out,
        cap,
        _TRUNCATE,
        L"%s\\py4gw-%04u%02u%02u-%02u%02u%02u-%lu-%lu",
        s_crash_dir,
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        ::GetCurrentProcessId(),
        ::GetCurrentThreadId());
}

void AppendInjectionLog(const char* line) {
    HANDLE file = ::CreateFileW(
        L"Py4GW_injection_log.txt",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    ::WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    ::CloseHandle(file);
}

void WriteGwText(const wchar_t* path, const char* text) {
    HANDLE file = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    ::WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    ::CloseHandle(file);
}

}  // namespace

CrashHandler& CrashHandler::Instance() {
    static CrashHandler instance;
    return instance;
}

std::string CrashHandler::CrashDirUtf8() const {
    return s_crash_dir_utf8;
}

bool CrashHandler::EnsureCrashDir() {
    if (s_crash_dir_ready) {
        return true;
    }

    wchar_t base[MAX_PATH] = {};
    const DWORD length = ::GetCurrentDirectoryW(MAX_PATH, base);
    if (length == 0 || length >= MAX_PATH) {
        const std::filesystem::path module_dir = py4gw::process_manager::GetModuleDirectory();
        if (module_dir.empty()) {
            return false;
        }
        wcsncpy_s(base, module_dir.c_str(), _TRUNCATE);
    }

    _snwprintf_s(s_crash_dir, MAX_PATH, _TRUNCATE, L"%s\\crashes", base);
    MakeDirTree(s_crash_dir);
    s_crash_dir_ready = true;

    char utf8[MAX_PATH * 2] = {};
    if (::WideCharToMultiByte(CP_UTF8, 0, s_crash_dir, -1, utf8, sizeof(utf8), nullptr, nullptr) > 0) {
        s_crash_dir_utf8 = utf8;
    }
    return true;
}

void CrashHandler::Initialize() {
    if (s_installed) {
        return;
    }

    s_installed = true;
    EnsureCrashDir();
    ClearCallbackFilterPolicy();
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    py4gw::RegisterPanicHandler(&CrashHandler::OnPanic, this);
    InstallPathA();
    InstallPathC();
    Logger::Instance().LogInfo("[CrashHandler] installed.");
}

void CrashHandler::Terminate() {
    if (!s_installed) {
        return;
    }

    s_installed = false;
    if (s_append_stack_fn) {
        py4gw::HookBase::DisableHooks(reinterpret_cast<void*>(s_append_stack_fn));
        py4gw::HookBase::RemoveHook(reinterpret_cast<void*>(s_append_stack_fn));
        s_append_stack_fn = 0;
        s_append_stack_orig = nullptr;
    }
    ::SetUnhandledExceptionFilter(s_prev_filter);
    py4gw::RegisterPanicHandler(nullptr, nullptr);
    RestoreCallbackFilterPolicy();
    ::InterlockedExchange(&s_handling, 0);
    Logger::Instance().LogInfo("[CrashHandler] torn down.");
}

void CrashHandler::ClearCallbackFilterPolicy() {
    HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return;
    }
    using GetFn = BOOL(WINAPI*)(LPDWORD);
    using SetFn = BOOL(WINAPI*)(DWORD);
    const auto get = reinterpret_cast<GetFn>(::GetProcAddress(kernel32, "GetProcessUserModeExceptionPolicy"));
    const auto set = reinterpret_cast<SetFn>(::GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy"));
    if (!get || !set) {
        return;
    }
    DWORD policy = 0;
    if (!get(&policy)) {
        return;
    }
    s_prev_policy = policy;
    if (set(policy & 0xFFFFFFFEu)) {
        s_policy_changed = true;
    }
}

void CrashHandler::RestoreCallbackFilterPolicy() {
    if (!s_policy_changed) {
        return;
    }
    HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return;
    }
    using SetFn = BOOL(WINAPI*)(DWORD);
    const auto set = reinterpret_cast<SetFn>(::GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy"));
    if (set) {
        set(s_prev_policy);
    }
    s_policy_changed = false;
}

void CrashHandler::InstallPathA() {
    s_prev_filter = ::SetUnhandledExceptionFilter(&CrashHandler::TopLevelFilter);
}

void CrashHandler::InstallPathC() {
    const auto* pattern = py4gw::Patterns::Get("crash.append_stack_anchor");
    if (!pattern) {
        Logger::Instance().LogWarning("[CrashHandler] Missing pattern: crash.append_stack_anchor");
        return;
    }

    const uintptr_t anchor = py4gw::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("append_stack_anchor", anchor, "crash")) {
        return;
    }

    const uintptr_t use = py4gw::Scanner::FindUseOfAddress(anchor, 0, py4gw::ScannerSection::Text);
    if (!Logger::AssertAddress("append_stack_anchor_use", use, "crash")) {
        return;
    }

    s_append_stack_fn = py4gw::Scanner::ToFunctionStart(use, 0x0fff);
    if (!Logger::AssertAddress("append_stack_target", s_append_stack_fn, "crash")) {
        Logger::Instance().LogWarning("[CrashHandler] Path C prologue miss; SEH only.");
        return;
    }

    void* target = reinterpret_cast<void*>(s_append_stack_fn);
    const int status = py4gw::HookBase::CreateHookRaw(target, reinterpret_cast<void*>(&CrashHandler::AppendStackDetour), &s_append_stack_orig);
    if (status != 0 || !s_append_stack_orig) {
        Logger::Instance().LogWarning("[CrashHandler] Path C CreateHook failed.");
        s_append_stack_fn = 0;
        s_append_stack_orig = nullptr;
        return;
    }

    py4gw::HookBase::EnableHooks(target);
    Logger::Instance().LogInfo("[CrashHandler] Path C attached.");
}

LONG WINAPI CrashHandler::TopLevelFilter(EXCEPTION_POINTERS* info) {
    Instance().OnException(info, "seh", false);
    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::OnPanic(
    void*,
    const char* expr,
    const char* message,
    const char* file,
    unsigned int line,
    const char* function) {
    strncpy_s(s_panic_expr, expr ? expr : "", _TRUNCATE);
    strncpy_s(s_panic_message, message ? message : "", _TRUNCATE);
    strncpy_s(s_panic_file, file ? file : "", _TRUNCATE);
    strncpy_s(s_panic_function, function ? function : "", _TRUNCATE);
    s_panic_line = line;
}

uintptr_t __cdecl CrashHandler::AppendStackDetour(void* debug_info, uint32_t a2, uint32_t a3,
                                                  uint32_t a4, CONTEXT* ctx, uint32_t a6,
                                                  uint32_t a7) {
    static_assert(sizeof(void*) == 4, "Py4GW crash handler is x86 only.");
    using Fn = uintptr_t(__cdecl*)(void*, uint32_t, uint32_t, uint32_t, CONTEXT*, uint32_t, uint32_t);

    if (!s_append_stack_orig) {
        return 0;
    }

    const uintptr_t result = reinterpret_cast<Fn>(s_append_stack_orig)(debug_info, a2, a3, a4, ctx, a6, a7);

    __try {
        const char* text = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(debug_info) + 0x20c);
        if (text && *text) {
            strncpy_s(s_gw_text, text, _TRUNCATE);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (ctx) {
        EXCEPTION_RECORD record = {};
        record.ExceptionCode = 0x80000003;
        record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        record.ExceptionAddress = reinterpret_cast<void*>(ctx->Eip);
        EXCEPTION_POINTERS info = { &record, ctx };
        Instance().OnException(&info, "gw_engine", false);
    }
    return result;
}

bool CrashHandler::OnException(EXCEPTION_POINTERS* info, const char* source, bool recoverable) {
    if (::InterlockedCompareExchange(&s_handling, 1, 0) != 0) {
        if (!recoverable) {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
        return false;
    }

    if (s_crash_dir_ready) {
        wchar_t stem[MAX_PATH] = {};
        wchar_t dump_path[MAX_PATH] = {};
        wchar_t json_path[MAX_PATH] = {};
        BuildStem(stem, MAX_PATH);
        _snwprintf_s(dump_path, MAX_PATH, _TRUNCATE, L"%s.dmp", stem);
        _snwprintf_s(json_path, MAX_PATH, _TRUNCATE, L"%s.json", stem);

        const wchar_t* dump_name = wcsrchr(dump_path, L'\\');
        dump_name = dump_name ? dump_name + 1 : dump_path;

        wchar_t gw_text_path[MAX_PATH] = {};
        const wchar_t* gw_text_name = L"";
        if (s_gw_text[0]) {
            _snwprintf_s(gw_text_path, MAX_PATH, _TRUNCATE, L"%s-gwtext.txt", stem);
            const wchar_t* slash = wcsrchr(gw_text_path, L'\\');
            gw_text_name = slash ? slash + 1 : gw_text_path;
        }

        char comment[512] = {};
        const DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
        _snprintf_s(
            comment,
            sizeof(comment),
            _TRUNCATE,
            "Py4GW | %s | 0x%08lX | panic:%s | %s",
            source,
            static_cast<unsigned long>(code),
            s_panic_expr[0] ? s_panic_expr : "?",
            s_panic_message[0] ? s_panic_message : "");

        WriteSidecar(info, json_path, dump_name, gw_text_name, source);
        WriteDump(info, dump_path, comment);
        if (gw_text_path[0]) {
            WriteGwText(gw_text_path, s_gw_text);
        }

        char dump_name_u8[MAX_PATH] = {};
        if (::WideCharToMultiByte(CP_UTF8, 0, dump_name, -1, dump_name_u8, sizeof(dump_name_u8), nullptr, nullptr) <= 0) {
            dump_name_u8[0] = 0;
        }
        char log_line[320] = {};
        _snprintf_s(
            log_line,
            sizeof(log_line),
            _TRUNCATE,
            "CRASH %s 0x%08lX -> see crashes\\%s\r\n",
            source,
            static_cast<unsigned long>(code),
            dump_name_u8);
        AppendInjectionLog(log_line);
    }

    if (recoverable) {
        ::InterlockedExchange(&s_handling, 0);
    }
    return true;
}

void CrashHandler::WriteSidecar(EXCEPTION_POINTERS* info, const wchar_t* json_path,
                                const wchar_t* dmp_name, const wchar_t* gwtext_name,
                                const char* source) {
    HANDLE file = ::CreateFileW(json_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    const DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
    const uintptr_t address =
        (info && info->ExceptionRecord) ? reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress) : 0;

    char escaped_dump[160] = {};
    char escaped_gw[1100] = {};
    char escaped_gw_name[MAX_PATH] = {};
    char escaped_panic_expr[600] = {};
    char escaped_panic_message[1200] = {};
    char escaped_panic_file[320] = {};
    char escaped_panic_function[180] = {};

    char dump_u8[MAX_PATH] = {};
    if (::WideCharToMultiByte(CP_UTF8, 0, dmp_name, -1, dump_u8, sizeof(dump_u8), nullptr, nullptr) > 0) {
        JsonEscape(escaped_dump, sizeof(escaped_dump), dump_u8);
    }
    JsonEscape(escaped_gw, sizeof(escaped_gw), s_gw_text);
    JsonEscape(escaped_panic_expr, sizeof(escaped_panic_expr), s_panic_expr);
    JsonEscape(escaped_panic_message, sizeof(escaped_panic_message), s_panic_message);
    JsonEscape(escaped_panic_file, sizeof(escaped_panic_file), s_panic_file);
    JsonEscape(escaped_panic_function, sizeof(escaped_panic_function), s_panic_function);
    if (gwtext_name && gwtext_name[0]) {
        char gw_name_u8[MAX_PATH] = {};
        if (::WideCharToMultiByte(CP_UTF8, 0, gwtext_name, -1, gw_name_u8, sizeof(gw_name_u8), nullptr, nullptr) > 0) {
            JsonEscape(escaped_gw_name, sizeof(escaped_gw_name), gw_name_u8);
        }
    }

    char buffer[4096] = {};
    JBuf json{ buffer, buffer + sizeof(buffer) };
    JAppend(json, "{\"source\":\"%s\",\"crash_class\":\"%s\",", source, ExceptionLabel(code));
    JAppend(json, "\"exception_code\":\"0x%08lX\",\"fault_address\":\"0x%08lX\",",
            static_cast<unsigned long>(code),
            static_cast<unsigned long>(address));
    JAppend(json, "\"faulting_tid\":%lu,\"dump_file\":\"%s\"", ::GetCurrentThreadId(), escaped_dump);
    if (escaped_gw_name[0]) {
        JAppend(json, ",\"gw_text_file\":\"%s\"", escaped_gw_name);
    }
    JAppend(
        json,
        ",\"panic\":{\"expr\":\"%s\",\"message\":\"%s\",\"file\":\"%s\",\"line\":%u,\"function\":\"%s\"},\"gw_text\":\"%s\"}\n",
        escaped_panic_expr,
        escaped_panic_message,
        escaped_panic_file,
        s_panic_line,
        escaped_panic_function,
        escaped_gw);

    DWORD written = 0;
    ::WriteFile(file, buffer, static_cast<DWORD>(json.p - buffer), &written, nullptr);
    ::CloseHandle(file);
}

void CrashHandler::WriteDump(EXCEPTION_POINTERS* info, const wchar_t* dmp_path, const char* comment) {
    HANDLE file = ::CreateFileW(dmp_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception_info = {};
    exception_info.ThreadId = ::GetCurrentThreadId();
    exception_info.ExceptionPointers = info;
    exception_info.ClientPointers = FALSE;

    MINIDUMP_USER_STREAM stream = {};
    stream.Type = CommentStreamA;
    stream.BufferSize = static_cast<ULONG>(strlen(comment) + 1);
    stream.Buffer = const_cast<char*>(comment);
    MINIDUMP_USER_STREAM_INFORMATION stream_info = { 1, &stream };
    const auto flags = static_cast<MINIDUMP_TYPE>(0x1041);

    __try {
        ::MiniDumpWriteDump(
            ::GetCurrentProcess(),
            ::GetCurrentProcessId(),
            file,
            flags,
            info ? &exception_info : nullptr,
            &stream_info,
            nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    ::CloseHandle(file);
}
