# Python Runtime

## Purpose

The Python runtime layer embeds CPython into the injected DLL so the project can expose a native API and execute Python-side scripts from within the game process.

Files:

- `include/base/python_runtime.h`
- `src/base/python_runtime.cpp`

## Runtime Ownership

The interpreter lifetime is represented by:

- `std::unique_ptr<py::scoped_interpreter> g_python_runtime`

This means the project relies on RAII to:

- initialize the interpreter
- finalize it on reset/destruction

## Embedded Module

The code defines:

- `PYBIND11_EMBEDDED_MODULE(Py4GW, m)`

Current module contents are intentionally small:

- `version()`
- `log(message)`

This module is importable from inside the embedded interpreter and acts as the seed API surface for future expansion.

## Initialization Flow

`python_runtime::Initialize()`:

1. constructs `py::scoped_interpreter`
2. imports `sys`
3. resolves the module directory via `process_manager`
4. prepends two paths to `sys.path`
   - module directory
   - `module_directory / "scripts"`
5. imports the embedded `Py4GW` module

This means Python scripts deployed beside the DLL or in a sibling `scripts/` folder become importable without extra environment setup.

## Error Handling

Initialization is wrapped in a `try/catch`.

On failure:

- logs `"Python initialization failed: ..."`
- resets the interpreter pointer
- returns `false`

This is appropriate because embedded Python initialization failures are recoverable at the project bootstrap level.

## Shutdown

`python_runtime::Shutdown()` simply resets the unique pointer.

That delegates interpreter finalization to pybind11/CPython teardown.

## Current Limitations

The Python layer is still minimal.

What it does not yet do:

- run user scripts automatically
- expose hook callbacks into Python
- manage GIL-sensitive long-lived callback registration
- maintain a Python-side plugin lifecycle
- expose logging levels beyond a single `log` helper

## Design Implication

The current Python runtime should be understood as:

- interpreter bootstrap
- import-path setup
- seed native module exposure

It is not yet a full scripting host framework.
