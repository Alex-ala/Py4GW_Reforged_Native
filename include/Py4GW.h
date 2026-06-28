#pragma once

#include "base/error_handling.h"

#include <windows.h>

#if defined(_MSC_VER)
#    define PY4GW_EXPORT __declspec(dllexport)
#else
#    define PY4GW_EXPORT
#endif

extern "C" {

PY4GW_EXPORT bool Py4GW_Initialize();
PY4GW_EXPORT void Py4GW_Shutdown();
PY4GW_EXPORT void Py4GW_RequestShutdown();

}

namespace PY4GW {

DWORD WINAPI RuntimeThread(LPVOID);

}
