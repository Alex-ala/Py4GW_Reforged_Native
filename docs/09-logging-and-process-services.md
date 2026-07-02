# Logging And Process Services

## Logger

Files:

- `include/base/logger.h`
- `src/base/logger.cpp`

## Purpose

The logger serves two jobs:

- human-readable runtime logging to a file and debugger output
- lightweight in-memory recording of scan and hook results

The on-screen console buffer is not owned by the logger; it lives in the
`System` service (see below). Every `Logger` write is mirrored into the
console buffer so native log lines remain visible on screen.

## Singleton Model

`Logger::Instance()` returns a process-wide singleton.

Synchronization:

- `log_mutex_` protects the file logging path and writes

The in-memory result maps are global statics outside the class:

- `s_scan_results`
- `s_hook_results`

These are not individually locked, so they are best understood as lightweight diagnostics rather than a rigorously synchronized telemetry subsystem.

## Log Output Destination

`SetLogFile()` accepts a path string.

If the path is relative:

- it is rooted under `process_manager::GetModuleDirectory()`

That makes log placement stable relative to the injected DLL.

## Log Levels

Supported methods:

- `LogInfo`
- `LogOk`
- `LogHook`
- `LogWarning`
- `LogError`
- `LogError(message, module_name)`

Formatting:

- timestamp
- bracketed level
- message

The message is written to:

- log file
- `OutputDebugStringA`

## Assert Helpers

The logger also exposes:

- `AssertAddress`
- `AssertHook`

These both:

- record a result in an in-memory map
- emit an error log on failure

This is a useful shape for reverse-engineering workflows where you want:

- a runtime success/failure view
- direct logging when a resolved symbol or hook installation fails

## System Service

Files (standard module split):

- `include/system/system.h` - all module-owned declarations
  (`ConsoleMessage`, `System`, `BorderlessState`, `WindowCfg`)
- `src/system/system.cpp` - singleton and internal machinery
  (borderless window subclassing)
- `src/system/system_methods.cpp` - public callable method bodies
- `src/system/system_bindings.cpp` - embedded Python module `PySystem`

## Purpose

`PY4GW::System` is a process-wide singleton that owns the script-facing
console features:

- `WriteConsoleMessage` appends a `ConsoleMessage` to the in-memory buffer
  (capped at 1000 entries). Console messages are screen-only by default.
- `SetOutputToFile` / `GetOutputToFile` toggle mirroring console writes
  into the injection log file via `Logger::WriteFileLine`. Default: off.
- `GetConsoleMessages` / `FilterConsoleMessages` (by module, level,
  substring) / `ClearConsoleMessages` service retrieval and clearing.
- `AppendConsoleMessage` is the buffer-only entry point used by `Logger`
  to mirror file log lines onto the console.

## Console Window Ownership

`System` also owns the console window visibility flags:

- `SetDrawConsole` / `GetDrawConsole` (full console)
- `SetDrawCompactConsole` / `GetDrawCompactConsole` (compact console)
- `ToggleConsole` / `ToggleCompactConsole`

`console_host_ui::Render` reads these each frame and writes back changes
made through the window close buttons. Consoles are therefore not
mandatory: scripts can hide or show them through `PySystem` without
triggering the shutdown confirmation, which only fires when the user
closes the last visible console from the UI in the same frame.

The shutdown confirmation modal itself is driven by a `System` flag:
`RequestShutdownPrompt` / `CancelShutdownPrompt` /
`IsShutdownPromptPending`. The console host renders the modal whenever
the flag is pending, so scripts can raise it explicitly
(`PySystem.request_shutdown_prompt()`) in addition to the UI close path.

## Account Anchor And Per-Account Settings

`System::UpdateAccountAnchor()` is stepped from the runtime update loop
(`UpdateLoopStep` in `src/Py4GW.cpp`) until the anchor resolves:

1. If a map is loaded, the account email is read from
   `CharContext::player_email`, stored as the anchor, and
   `<dll_dir>/settings/<email>/` is created. The path is always rooted at
   the DLL directory (`process_manager::GetModuleDirectory()`), never the
   game process working directory.
2. If not in a map but sitting in character select
   (`System::InCharacterSelectScreen()`, ported from the legacy
   `InCharacterSelectScreen`: pregame chars present plus
   `kCheckUIState == 2`), Enter is pushed via `KeyHandler` every 2 seconds
   to log the selected character in.

ImGui ini persistence is held off until the anchor exists: the imgui
manager initializes with `io.IniFilename = nullptr` and, once
`System::HasAccountEmail()` is true, points ImGui at
`settings/<email>/imgui.ini` (loading it if present). The same directory
is the intended home for the autoexec script path and the remaining
console persistence variables.

Python surface (module level on `PySystem`): `in_character_select_screen`,
`has_account_email`, `get_account_email`, `get_settings_directory`.

## Migrated Legacy Surfaces

The following legacy `Py4GW` (old project) binding groups now live in the
System module:

- `bind_Console` extras: `System::GetCredits`, `System::GetLicense`,
  `System::ChangeWorkingDirectory` (message logging itself is
  `WriteConsoleMessage`).
- `get_tick_count64`: `System::GetTickCount64`. Frame-based like the
  legacy version: `System::UpdateFrameTimestamp` is stamped at the top of
  the render `DrawLoop` (`src/Py4GW.cpp`) and the getter returns that
  frame timestamp, falling back to `::GetTickCount64()` before the first
  frame.
- `bind_Environment`: `PySystem.environment` (`get_gw_window_handle` via
  `MemoryManager::GetGWWindowHandle`, `get_projects_path`).
- `bind_Window`: `PY4GW::WindowCfg`, declared in `system.h` as part of
  the system module, a parity port of the legacy `WindowCfg.h` including
  the true-borderless window subclassing. Bound as `PySystem.window`.
- `bind_ScriptControl`: `PySystem.script_control`, routing to
  `python_runtime` (`load`, `run`, `stop`, `pause`, `resume`, `status`,
  `defer_load_and_run`, `defer_stop_load_and_run`, `defer_stop_and_run`).

## Python Surface

- `PySystem` binds the `MessageType` enum and `ConsoleMessage` type at
  module level, plus `get_tick_count64`, `get_credits`, `get_license`,
  `change_working_directory`, and the shutdown prompt helpers
  (`request_shutdown_prompt`, `cancel_shutdown_prompt`,
  `is_shutdown_prompt_pending`).
- `PySystem.console` holds all console handling: `write` (level string or
  `MessageType` overloads), `get_messages`, `filter_messages`,
  `clear_messages`, `set_output_to_file` / `get_output_to_file`, and the
  window visibility helpers (`set/get_draw_console`,
  `set/get_draw_compact_console`, `toggle_console`,
  `toggle_compact_console`).
- Further submodules: `PySystem.environment`, `PySystem.window`,
  `PySystem.script_control` (see the migration list above).
- `Py4GW.Console` remains as the legacy logging surface (`log`, `info`,
  `warning`, `error`, `notice`, `success`, `debug`, `performance`,
  `print`) and now routes through `System::WriteConsoleMessage`. The per-call
  `export` argument was removed; file output is controlled solely by the
  `output_to_file` toggle.

## Process Manager

Files:

- `include/base/process_manager.h`
- `src/base/process_manager.cpp`

## Purpose

The process manager is intentionally tiny. It stores the module handle assigned during DLL attach and exposes the module directory.

Public functions:

- `SetModuleHandle`
- `GetModuleHandle`
- `GetModuleDirectory`

## Why It Exists

Several subsystems need a common notion of the DLL location:

- logger for file path rooting
- Python runtime for `sys.path`
- patterns loader for `offsets/`
- runtime thread for final module unload

Centralizing that lookup prevents repeated `GetModuleFileNameW` logic and keeps path-rooting consistent.

## Limitations

The process manager is just a handle holder. It does not:

- inspect other modules
- enumerate processes
- provide thread or memory utilities

Its name is slightly broader than its current scope, but its implementation is straightforward.
