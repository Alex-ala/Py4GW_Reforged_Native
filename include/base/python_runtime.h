#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <string>

namespace py4gw::python_runtime {

enum class ScriptState {
    Stopped,
    Running,
    Paused
};

bool Initialize();
void Shutdown();
void Update();
void Draw();
void ProcessDeferredActions();

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

}  // namespace py4gw::python_runtime
