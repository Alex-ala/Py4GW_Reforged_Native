#include "base/error_handling.h"

#include "Py4GW.h"
#include "GW/GuildWars.h"
#include "GW/render/render_methods.h"
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
    if (py4gw::python_runtime::GetScriptState() == py4gw::python_runtime::ScriptState::Stopped) {
        return;
    }

    Logger::Instance().LogInfo("Stopping running script before shutdown.");
    py4gw::python_runtime::StopScript();
}

void BeginShutdown() {
    if (g_shutdown_requested) {
        return;
    }

    StopRunningScriptForShutdown();
    g_shutdown_requested = true;
}

void UpdateLoopStep() {
    py4gw::python_runtime::ExecutePythonUpdate();
    ::Sleep(10);
}

void DrawLoop(IDirect3DDevice9* device) {
    if (!g_running) {
        return;
    }
    if (!py4gw::imgui::BeginFrame(device)) {
        return;
    }

    py4gw::python_runtime::ExecutePythonDraw();
    py4gw::python_runtime::ProcessDeferredActions();

    bool request_shutdown = false;
    py4gw::imgui::RenderConsoleUi(&request_shutdown);
    py4gw::imgui::EndFrame(device);
}

void OnReset(IDirect3DDevice9*) {
    py4gw::imgui::InvalidateDeviceObjects();
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
    if (!py4gw::Scanner::Initialize()) {
        Logger::Instance().LogError("Scanner initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "bootstrap", "patterns_initialize");
    Logger::Instance().LogInfo("Initializing patterns.");
    if (!py4gw::Patterns::Initialize()) {
        Logger::Instance().LogError("Pattern initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "python_runtime", "initialize");
    Logger::Instance().LogInfo("Initializing Python runtime.");
    if (!py4gw::python_runtime::Initialize()) {
        Logger::Instance().LogError("Python runtime initialization failed.");
        return false;
    }

    CrashHandler::SetContext("startup", "gw", "initialize");
    Logger::Instance().LogInfo("Initializing Guild Wars modules.");
    if (!gw::Initialize()) {
        Logger::Instance().LogError("Guild Wars hook initialization failed.");
        py4gw::python_runtime::Shutdown();
        return false;
    }

    CrashHandler::SetContext("startup", "crash_handler", "initialize");
    Logger::Instance().LogInfo("Initializing crash handler.");
    CrashHandler::Instance().Initialize();
    CrashHandler::SetContext("runtime", "bootstrap", "initialized");

    py4gw::imgui::SetShutdownCallback(&BeginShutdown);
    gw::render::SetResetCallback(&OnReset);
    gw::render::SetRenderCallback(&DrawLoop);

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

    gw::render::SetRenderCallback(nullptr);
    gw::render::SetResetCallback(nullptr);

    CrashHandler::SetContext("shutdown", "imgui", "shutdown");
    Logger::Instance().LogInfo("Shutting down ImGui.");
    py4gw::imgui::Shutdown();
    CrashHandler::SetContext("shutdown", "gw", "shutdown");
    Logger::Instance().LogInfo("Shutting down Guild Wars modules.");
    gw::Shutdown();
    CrashHandler::SetContext("shutdown", "python_runtime", "shutdown");
    Logger::Instance().LogInfo("Shutting down Python runtime.");
    py4gw::python_runtime::Shutdown();
    g_running = false;
    g_shutdown_requested = false;
    CrashHandler::SetContext("shutdown", "bootstrap", "shutdown_complete");
    Logger::Instance().LogInfo("Py4GW shutdown complete.");
}

void Py4GW_RequestShutdown() {
    BeginShutdown();
}

}

namespace py4gw {

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

