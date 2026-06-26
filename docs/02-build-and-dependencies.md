# Build And Dependencies

## Toolchain Constraints

The project is explicitly restricted to:

- Windows
- 32-bit build target
- C++20

This is enforced in `CMakeLists.txt`:

- non-Windows configuration is rejected
- non-32-bit configuration is rejected with the message to use `-A Win32`

## Build System

The project uses CMake to generate a Visual Studio/MSBuild build.

Main build products:

- `imgui` static library
- `Py4GW` shared library

`Py4GW` is built as a DLL with:

- output name `Py4GW`
- no `lib` prefix

## Source Sets

### `imgui` target

The project builds a local static library for the Dear ImGui core and two backends:

- `imgui.cpp`
- `imgui_draw.cpp`
- `imgui_tables.cpp`
- `imgui_widgets.cpp`
- `imgui_impl_dx9.cpp`
- `imgui_impl_win32.cpp`

Optional:

- `imgui_demo.cpp` when `PY4GW_BUILD_IMGUI_DEMO` is enabled

### `Py4GW` target

Project-owned sources currently include:

- `src/base/file_scanner.cpp`
- `src/GW/render/render.cpp`
- `src/base/patterns.cpp`
- `src/dllmain.cpp`
- `src/base/imgui_manager.cpp`
- `src/base/logger.cpp`
- `src/base/process_manager.cpp`
- `src/Py4GW.cpp`
- `src/base/python_runtime.cpp`
- `src/base/scanner.cpp`

## Include Layout

`Py4GW` includes:

- `include/`
- `third_party/`

The `third_party/` include path is necessary because:

- `pybind11` headers are consumed directly
- `imgui` headers are consumed directly
- `MinHook.h` is included
- `nlohmann/json.hpp` is used by the pattern loader

## Linked Libraries

### `imgui`

- `d3d9.lib`

### `Py4GW`

- `pybind11::embed`
- `imgui`
- `minhook`
- `Python3::Python`
- `d3d9.lib`

## Compile Definitions

The project uses standard Windows-oriented reductions:

- `NOMINMAX`
- `WIN32_LEAN_AND_MEAN`

`Py4GW` also enables:

- `_CRT_SECURE_NO_WARNINGS`
- `PYBIND11_DETAILED_ERROR_MESSAGES`

## Post-Build Behavior

The build copies `offsets/` beside the built DLL:

- source: repo `offsets/`
- destination: `$<TARGET_FILE_DIR:Py4GW>/offsets`

This is important because `Patterns::Initialize()` defaults to:

- `module_directory / "offsets"`

Without that copy step, runtime pattern loading would depend on the current working directory or manual file placement.

## Dependency Roles

### `pybind11`

Used for:

- embedded Python interpreter lifetime
- embedded `Py4GW` module registration

### `imgui`

Used for:

- overlay UI rendering
- DX9 device integration
- Win32 input integration

### `minhook`

Used for:

- detouring Guild Wars render/reset targets

### `nlohmann/json`

Used for:

- parsing `offsets/*.json` into runtime pattern objects

## Expected Build Command

Typical configure/build flow:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config RelWithDebInfo
```

## Observed Build Characteristics

The repo already contains a generated `build/` tree. That is convenient for local iteration, but it should still be treated as generated output, not hand-maintained project source.



