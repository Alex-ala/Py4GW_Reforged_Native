# Logging And Process Services

## Logger

Files:

- `include/base/logger.h`
- `src/base/logger.cpp`

## Purpose

The logger serves two jobs:

- human-readable runtime logging to a file and debugger output
- lightweight in-memory recording of scan and hook results

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
