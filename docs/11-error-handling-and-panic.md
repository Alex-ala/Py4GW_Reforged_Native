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

What the macros do:

- Record file, line, function, failed expression, and optional message.
- Forward panic metadata to the crash handler through `RegisterPanicHandler(...)`.
- Emit crash artifacts through the process-wide crash handler.
- Terminate the process after a fatal invariant violation.

Design intent:

- New modules should inherit the panic API automatically through normal project header usage.
- Panic capture must stay process-wide and centralized.
- Assertions are for programming errors and bad internal state, not user-facing validation.

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

Do not:

- Call `abort()` directly in project code.
- Introduce ad-hoc assert macros in module-local headers.
- Bypass the crash pipeline for fatal internal failures.
