# Repository Overview

## Purpose

`Py4GW_Reforged` is a Windows-only 32-bit injected DLL project targeting Guild Wars. It combines four main responsibilities:

- Inject a runtime thread from `DllMain`
- Hook the Guild Wars Direct3D 9 render pipeline
- Host an embedded Python interpreter through pybind11
- Render an ImGui overlay inside the game process

The project is small and centralized. Most project-owned logic is in a handful of files under `src/` and `include/`.

## Top-Level Layout

### `include/`

Public and internal headers used by the project source tree.

- `Py4GW.h`
  Exports the DLL entry points and runtime thread declaration.
- `include/base/`
  Utility/runtime support services, including logging and error handling.
- `include/GW/`
  Guild Wars specific scanner, hook, and pattern-loading interfaces.

### `src/`

Main implementation code.

- `Py4GW.cpp`
  Central runtime initialization and shutdown flow.
- `dllmain.cpp`
  DLL process attach/detach entry point.
- `src/base/`
  Python runtime, logging, crash handling, scanner support, and other general helpers.
- `src/GW/`
  Scanner, file scanner, hook setup, and pattern loader code.

### `offsets/`

Runtime JSON pattern data.

- `render.json`
  Current pattern namespace for render-hook related lookups.

### `third_party/`

Vendored dependencies.

- `pybind11`
- `imgui`
- `minhook`
- `nlohmann/json.hpp`

### `build/`

Generated CMake and MSBuild output. This is not source-of-truth code.

## Code Ownership Boundaries

The repo currently has a clean split between project-owned orchestration and vendor-owned infrastructure.

Project-owned logic:

- Bootstrap
- Hook selection
- Binary scanning
- Pattern loading
- Overlay wiring
- Python interpreter wiring
- Logging

Vendor-owned logic:

- Hook patching internals through MinHook
- Embedded Python C++ bindings through pybind11
- Immediate-mode UI framework and DX9/Win32 backends through ImGui
- JSON parsing through nlohmann/json

## High-Level Runtime Flow

1. The DLL loads into the target process.
2. `DllMain` stores the module handle and spawns `RuntimeThread`.
3. `RuntimeThread` calls `Py4GW_Initialize`.
4. Initialization sets up logging, Python, scanner state, JSON patterns, render hook targets, and ImGui callbacks.
5. Guild Wars render/reset hooks start invoking project callbacks.
6. ImGui initializes lazily on first usable render device.
7. The runtime thread polls for shutdown until requested.
8. Shutdown detaches callbacks, tears down ImGui, unhooks render functions, shuts down Python, and unloads the DLL.

## Important Architectural Characteristic

The project is not trying to abstract every binary-analysis action behind generic data-driven logic. The current design uses JSON only for pattern data, while call-site code still owns meaning and composition. That is visible in `src/GW/render/render.cpp`, where the caller decides whether a loaded `offset` should be used as:

- the displacement passed to `Scanner::Find`, or
- the scan range passed to `Scanner::ToFunctionStart`

That is a deliberate design choice and should be preserved unless the project later chooses to standardize a more rigid schema.


