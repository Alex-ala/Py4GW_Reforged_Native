#include "base/error_handling.h"

#include "Py4GW.h"
#include "GW/GuildWars.h"
#include "GW/render/render.h"
#include "base/CrashHandler.h"
#include "imgui/imgui_manager.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/process_manager.h"
#include "base/python_runtime.h"
#include "base/scanner.h"

#include <d3d9.h>
#include <mutex>
namespace {

std::mutex g_runtime_mutex;
bool g_running = false;
bool g_shutdown_requested = false;

void StopRunningScriptForShutdown() {
    if (PY4GW::python_runtime::GetScriptState() == PY4GW::python_runtime::ScriptState::Stopped) {
        return;
    }

    Logger::Instance().LogInfo("Stopping running script before shutdown.");
    PY4GW::python_runtime::StopScript();
}

void BeginShutdown() {
    if (g_shutdown_requested) {
        return;
    }

    StopRunningScriptForShutdown();
    g_shutdown_requested = true;
}

void UpdateLoopStep() {
    PY4GW::python_runtime::ExecutePythonUpdate();
    ::Sleep(10);
}

void DrawLoop(IDirect3DDevice9* device) {
    if (!g_running) {
        return;
    }
    if (!PY4GW::imgui::BeginFrame(device)) {
        return;
    }

    PY4GW::python_runtime::ExecutePythonDraw();
    PY4GW::python_runtime::ProcessDeferredActions();

    bool request_shutdown = false;
    PY4GW::imgui::RenderConsoleUi(&request_shutdown);
    PY4GW::imgui::EndFrame(device);
}

void OnReset(IDirect3DDevice9*) {
    PY4GW::imgui::InvalidateDeviceObjects();
}

}  // namespace

extern "C" {

bool Py4GW_Initialize() {
    std::scoped_lock guard(g_runtime_mutex);
    if (g_running) {
        return true;
    }

    Logger::Instance().SetLogFile("Py4GW_injection_log.txt");
    g_shutdown_requested = false;
    CrashHandler::SetContext("startup", "bootstrap", "scanner_initialize");
    Logger::Instance().LogInfo("Initializing scanner.");
    if (!PY4GW::Scanner::Initialize()) {
        Logger::Instance().LogError("Scanner initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "bootstrap", "patterns_initialize");
    Logger::Instance().LogInfo("Initializing patterns.");
    if (!PY4GW::Patterns::Initialize()) {
        Logger::Instance().LogError("Pattern initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "python_runtime", "initialize");
    Logger::Instance().LogInfo("Initializing Python runtime.");
    if (!PY4GW::python_runtime::Initialize()) {
        Logger::Instance().LogError("Python runtime initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "gw", "initialize");
    Logger::Instance().LogInfo("Initializing Guild Wars modules.");
    if (!GW::Initialize()) {
        Logger::Instance().LogError("Guild Wars hook initialization failed.");
        PY4GW::python_runtime::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "crash_handler", "initialize");
    Logger::Instance().LogInfo("Initializing crash handler.");
    CrashHandler::Instance().Initialize();
    CrashHandler::SetContext("runtime", "bootstrap", "initialized");

    PY4GW::imgui::SetShutdownCallback(&BeginShutdown);
    GW::render::SetResetCallback(&OnReset);
    GW::render::SetRenderCallback(&DrawLoop);

    g_running = true;
    Logger::Instance().LogInfo("Py4GW initialized.");
    return true;
}

void Py4GW_Shutdown() {
    std::scoped_lock guard(g_runtime_mutex);
    if (!g_running) {
        return;
    }

    StopRunningScriptForShutdown();

    GW::render::SetRenderCallback(nullptr);
    GW::render::SetResetCallback(nullptr);

    CrashHandler::SetContext("shutdown", "imgui", "shutdown");
    Logger::Instance().LogInfo("Shutting down ImGui.");
    PY4GW::imgui::Shutdown();
    CrashHandler::SetContext("shutdown", "gw", "shutdown");
    Logger::Instance().LogInfo("Shutting down Guild Wars modules.");
    GW::Shutdown();
    CrashHandler::SetContext("shutdown", "python_runtime", "shutdown");
    Logger::Instance().LogInfo("Shutting down Python runtime.");
    PY4GW::python_runtime::Shutdown();
    g_running = false;
    g_shutdown_requested = false;
    CrashHandler::SetContext("shutdown", "bootstrap", "shutdown_complete");
    Logger::Instance().LogInfo("Py4GW shutdown complete.");
}

void Py4GW_RequestShutdown() {
    BeginShutdown();
}

}

namespace PY4GW {

DWORD WINAPI RuntimeThread(LPVOID) {
    if (!Py4GW_Initialize()) {
        return 1;
    }

    while (!g_shutdown_requested) {
        UpdateLoopStep();
    }

    // Let the frame that requested shutdown unwind before tearing down hooks and Python.
    ::Sleep(50);
    Py4GW_Shutdown();
    if (process_manager::GetModuleHandle()) {
        ::FreeLibraryAndExitThread(process_manager::GetModuleHandle(), 0);
    }
    return 0;
}

}

