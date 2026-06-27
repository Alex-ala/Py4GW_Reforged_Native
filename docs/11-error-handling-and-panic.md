# Error Handling And Panic

This project uses a single project-owned error-handling entry header declared in `include/base/error_handling.h`.

Rules:

- Include `base/error_handling.h` from every project-owned header.
- The `Py4GW` and `imgui_addons` targets force-include `base/error_handling.h` at compile time through CMake.
- Use `PY4GW_ASSERT(expr)` for invariants that must always hold.
- Use `PY4GW_ASSERT_MSG(expr, "reason")` when the failure needs operator context.
- Use `PY4GW_REQUIRE(expr, "reason")` for preconditions at module boundaries.
- Use `PY4GW_PANIC("reason")` for unrecoverable states that are not simple boolean assertions.
- Use `PY4GW_UNREACHABLE("reason")` for impossible control-flow paths.
- Keep crash capture process-wide and initialized for as much of process lifetime as practical.
- Attribute module and operation context before risky startup, hook, patch, callback, and shutdown work.

What the macros do:

- Record file, line, function, failed expression, and optional message.
- Forward panic metadata to the crash handler through `RegisterPanicHandler(...)`.
- Emit crash artifacts through the process-wide crash handler.
- Terminate the process after a fatal invariant violation.

Design intent:

- New modules should inherit the panic API automatically through normal project header usage.
- Panic capture must stay process-wide and centralized.
- Assertions are for programming errors and bad internal state, not user-facing validation.

## Crash Sources

Crash sidecars currently report one of these sources:

- `py4gw_panic_handler`
  A project panic or failed `PY4GW_*` invariant reached the fatal handler. This is high-signal for project code.
- `structured_exception_handler`
  The process unhandled-exception filter observed the crash. This is a generic Windows fallback.
- `vectored_exception_handler`
  The vectored exception handler saw an exception first. This is useful telemetry, but it is not the preferred ownership signal by itself.
- `guild_wars_crash_handler`
  The Guild Wars crash path was intercepted. This is usually the most useful source for runtime crashes that happen inside the game or while our hooks are active.

Prefer `py4gw_panic_handler` and `guild_wars_crash_handler` when reasoning about ownership. The other sources are still useful, but they are lower-confidence attribution signals.

## Crash Artifacts

The crash handler emits sidecars under the crash output directory.

Current artifact types:

- pretty-printed `.json` summary with exception code, fault address, thread id, source, panic metadata, and crash context
- `-stack.txt` text stack trace when stack walking succeeds
- `-gwtext.txt` captured Guild Wars crash text when the game crash handler path is involved
- optional `.dmp` file for high-signal sources only

Current dump policy:

- `py4gw_panic_handler`: writes a dump
- `structured_exception_handler`: writes a dump
- `vectored_exception_handler`: no dump by default
- `guild_wars_crash_handler`: no dump by default

This keeps common crash artifacts small enough to keep while preserving dump generation for the sources most useful for deep debugging.

## Crash Context

Crash context is required if we want logs to identify where our code failed.

Use:

- `CrashHandler::SetContext(...)` for broad lifecycle phases
- `CrashContextScope` for temporary scoped operations

Recommended context fields:

- `phase`
- `module`
- `operation`
- `detail`

Examples of good context:

- startup, `render`, `initialize`
- shutdown, `camera`, `disable_hooks`
- runtime, `memory_patcher`, `toggle_patch`

Do not rely on raw exception addresses alone when the same information can be attached proactively.

## Shutdown Diagnostics

Shutdown crashes are common in injected code because teardown often crosses hooks, callbacks, and shared synchronization state.

When diagnosing shutdown failures:

- trust `phase = shutdown` plus `module` and `operation` context first
- check whether the module disabled entry points before deleting state they use
- check whether the module waited for in-flight detours or callbacks to drain
- check whether shutdown order is the reverse of safe dependency ownership, not just the reverse of initialization

If a crash log points at shutdown with a module-specific context, treat that as evidence of a teardown contract failure until proven otherwise.

Implementation references:

- `include/base/error_handling.h`
- `include/base/panic.h`
- `include/base/CrashHandler.h`
- `src/base/panic.cpp`
- `src/base/CrashHandler.cpp`

Usage example:

```cpp
#include "GW/render/render.h"

void RenderBootstrap() {
    PY4GW_REQUIRE(py4gw::Scanner::Initialize(), "scanner initialization failed");
    PY4GW_REQUIRE(py4gw::Patterns::Initialize(), "pattern initialization failed");
}
```

Scoped context example:

```cpp
void InitializeRenderHooks() {
    py4gw::CrashContextScope scope("startup", "render", "enable_hooks");
    PY4GW_REQUIRE(EnableHooks(), "render hook enable failed");
}
```

Do not:

- Call `abort()` directly in project code.
- Introduce ad-hoc assert macros in module-local headers.
- Bypass the crash pipeline for fatal internal failures.
- Add one-off crash log formatting in a module when the shared crash handler can own it once.
