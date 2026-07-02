#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <string>

namespace PY4GW::python_runtime {

enum class ScriptState {
    Stopped,
    Running,
    Paused
};

bool Initialize();
void Shutdown();
void ExecutePythonUpdate();
void ExecutePythonDraw();
void ProcessDeferredActions();

// Widget manager: an always-on second script host (Py4GW_widget_manager.py),
// independent of the user/console script.
bool StartWidgetManager();
void StopWidgetManager();
void ExecuteWidgetManagerUpdate();
void ExecuteWidgetManagerDraw();
ScriptState GetWidgetManagerState();
std::string GetWidgetManagerStatus();

bool SetSelectedScriptPath(const std::string& path);
const std::string& GetSelectedScriptPath();

bool LoadSelectedScript();
bool StartSelectedScript();
bool RunScript();
void StopScript();
bool PauseScript();
bool ResumeScript();
std::string GetScriptStatus();
double GetScriptElapsedMilliseconds();

void DeferLoadAndRun(const std::string& path, int delay_ms = 1000);
void DeferStopLoadAndRun(const std::string& path, int delay_ms = 1000);
void DeferStopAndRun(int delay_ms = 1000);

bool ExecuteCommand(const std::string& command);
ScriptState GetScriptState();
const char* GetScriptStateLabel();
bool HasLoadedScript();

}  // namespace PY4GW::python_runtime
