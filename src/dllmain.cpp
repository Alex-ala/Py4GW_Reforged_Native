#include "base/error_handling.h"

#include "Py4GW.h"
#include "base/logger.h"
#include "base/process_manager.h"

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        ::DisableThreadLibraryCalls(module);
        py4gw::process_manager::SetModuleHandle(module);
        HANDLE thread_handle = ::CreateThread(nullptr, 0, &py4gw::RuntimeThread, nullptr, 0, nullptr);
        if (thread_handle != nullptr) {
            ::CloseHandle(thread_handle);
        } else {
            Logger::Instance().SetLogFile("Py4GW_injection_log.txt");
            Logger::Instance().LogError("Unable to create main thread.");
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            Py4GW_Shutdown();
        }
        break;
    default:
        break;
    }
    return TRUE;
}
