#include "base/error_handling.h"

#include "base/panic.h"

#include "base/CrashHandler.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace py4gw {

namespace {

LogHandler s_log_handler = nullptr;
void* s_log_handler_context = nullptr;
PanicHandler s_panic_handler = nullptr;
void* s_panic_handler_context = nullptr;

}  // namespace

void RegisterLogHandler(LogHandler handler, void* context) {
    s_log_handler = handler;
    s_log_handler_context = context;
}

void RegisterPanicHandler(PanicHandler handler, void* context) {
    s_panic_handler = handler;
    s_panic_handler_context = context;
}

[[noreturn]] void FatalAssert(
    const char* expr,
    const char* file,
    unsigned int line,
    const char* function) {
    FatalAssertMsg(expr, nullptr, file, line, function);
}

[[noreturn]] void FatalAssertMsg(
    const char* expr,
    const char* message,
    const char* file,
    unsigned int line,
    const char* function) {
    if (s_panic_handler) {
        s_panic_handler(s_panic_handler_context, expr, message, file, line, function);
    }

    CONTEXT context = {};
    ::RtlCaptureContext(&context);
    EXCEPTION_RECORD record = {};
    record.ExceptionCode = 0xE0000001;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_IX86)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#else
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#endif
    EXCEPTION_POINTERS pointers = { &record, &context };
    CrashHandler::Instance().OnException(&pointers, "panic", false);
    ::TerminateProcess(::GetCurrentProcess(), 1);
}

[[noreturn]] void Panic(
    const char* message,
    const char* file,
    unsigned int line,
    const char* function) {
    FatalAssertMsg(nullptr, message, file, line, function);
}

void __cdecl LogMessage(
    const char* level,
    const char* file,
    unsigned int line,
    const char* function,
    const char* fmt,
    ...) {
    va_list args;
    va_start(args, fmt);
    LogMessageV(level, file, line, function, fmt, args);
    va_end(args);
}

void __cdecl LogMessageV(
    const char* level,
    const char* file,
    unsigned int line,
    const char* function,
    const char* fmt,
    va_list args) {
    if (!s_log_handler) {
        return;
    }

    char message[1024] = {};
    vsnprintf(message, sizeof(message), fmt, args);
    s_log_handler(s_log_handler_context, level, message, file, line, function);
}

}  // namespace py4gw
