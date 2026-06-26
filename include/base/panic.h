#pragma once

#include <cstdarg>

#define PY4GW_PANIC(message) ((void)(py4gw::Panic((message), __FILE__, (unsigned)__LINE__, __FUNCTION__), 0))
#define PY4GW_ASSERT(expr) ((void)(!!(expr) || (py4gw::FatalAssert(#expr, __FILE__, (unsigned)__LINE__, __FUNCTION__), 0)))
#define PY4GW_ASSERT_MSG(expr, message) ((void)(!!(expr) || (py4gw::FatalAssertMsg(#expr, (message), __FILE__, (unsigned)__LINE__, __FUNCTION__), 0)))
#define PY4GW_REQUIRE(expr, message) PY4GW_ASSERT_MSG((expr), (message))
#define PY4GW_UNREACHABLE(message) PY4GW_PANIC((message))

namespace py4gw {

typedef void (*LogHandler)(
    void* context,
    const char* level,
    const char* message,
    const char* file,
    unsigned int line,
    const char* function);

typedef void (*PanicHandler)(
    void* context,
    const char* expr,
    const char* message,
    const char* file,
    unsigned int line,
    const char* function);

void RegisterLogHandler(LogHandler handler, void* context);
void RegisterPanicHandler(PanicHandler handler, void* context);

[[noreturn]] void FatalAssert(
    const char* expr,
    const char* file,
    unsigned int line,
    const char* function);

[[noreturn]] void FatalAssertMsg(
    const char* expr,
    const char* message,
    const char* file,
    unsigned int line,
    const char* function);

[[noreturn]] void Panic(
    const char* message,
    const char* file,
    unsigned int line,
    const char* function);

void __cdecl LogMessage(
    const char* level,
    const char* file,
    unsigned int line,
    const char* function,
    const char* fmt,
    ...);

void __cdecl LogMessageV(
    const char* level,
    const char* file,
    unsigned int line,
    const char* function,
    const char* fmt,
    va_list args);

}  // namespace py4gw
