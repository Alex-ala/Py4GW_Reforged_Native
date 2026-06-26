# Py4GW Reforged Documentation

This folder documents the project-owned code in `Py4GW_Reforged`.

Scope:

- Project bootstrap and runtime lifecycle
- Guild Wars hook and scanner layers
- JSON-backed pattern loading
- Embedded Python setup
- ImGui integration and Win32 input interception
- Logging and small utility services
- Build configuration and repository layout

Out of scope for detailed internal documentation:

- `third_party/`
- `build/`

Those directories are still described at a high level in the repository layout notes because they affect how the project builds and runs, but their internal implementation is not documented file-by-file here.

Documents:

- `01-repository-overview.md`
- `02-build-and-dependencies.md`
- `03-runtime-lifecycle.md`
- `04-gw-hooking-and-render-path.md`
- `05-scanner-and-file-scanner.md`
- `06-pattern-json-system.md`
- `07-python-runtime.md`
- `08-imgui-and-input-layer.md`
- `09-logging-and-process-services.md`
- `10-current-observations-and-risks.md`
- `11-error-handling-and-panic.md`
- `12-project-style-guide.md`
