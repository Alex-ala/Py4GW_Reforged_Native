# Project Style Guide

This document defines mandatory conventions for `Py4GW_Reforged` project-owned code.

## Core Include Policy

- Every project-owned header must include `base/error_handling.h` immediately after `#pragma once`.
- Build targets must force-include `base/error_handling.h` so new translation units inherit the panic layer even if a local include is missed.
- New shared macros or fatal-runtime policy belong in `base/error_handling.h` or headers it owns.
- Third-party code under `third_party/` is excluded from this rule.

## Fatal Error Policy

- Fatal internal failures must use the `PY4GW_*` panic/assert macros.
- Process-wide crash capture is a required subsystem, not an optional utility.
- Do not duplicate panic registration logic inside feature modules.

## Pattern And Hooking Policy

- Addresses and hooks must prefer the JSON-backed pattern system.
- Validate resolved addresses with project assertions before installing hooks.
- Avoid hardcoded addresses unless the module explicitly documents why they are unavoidable.

## Logging Policy

- Use the project logger for runtime diagnostics.
- Use panic/assert macros for invariant failures, not plain log lines.
- If a failure must stop the process, it is not a logger-only event.

## ImGui And UI Policy

- Keep UI composition in dedicated UI functions instead of embedding large trees directly into render plumbing.
- Addons and demos must stay separated so dependency tracking remains explicit.
- Project UI code may demo functionality, but demos must not redefine core runtime policy.

## Porting Policy

- Port behavior intentionally; do not copy foreign modules blindly.
- Refit imported code to this project's scanner, logger, lifecycle, and panic systems.
- Preserve the smallest dependency surface needed for the current project stage.
- Do not add functionality or behavioral redesign during migration unless an existing project doc explicitly requires it.
- If a migration must intentionally differ from legacy behavior, document that deviation explicitly instead of mixing it into parity work.
- Shared GW structs and helper methods must live in shared GW locations such as `GW/context/`, not in fake module folders.
- If a migrated manager depends on unmigrated shared legacy code, migrate that prerequisite first instead of adding a compatibility shim or local substitute.
- Do not use temporary glue to hide missing legacy dependencies; parity work must stay directly traceable to migrated legacy code.

## Documentation Encoding Policy

- Project documentation should use plain ASCII unless there is a clear reason not to.
- Do not introduce mojibake or mixed-encoding text into `docs/`.
- Treat mojibake as a documentation defect and fix it when found.
- When importing text from other sources, normalize punctuation and encoding before committing it.

## Review Checklist

- Does the header include `base/error_handling.h`?
- Are fatal invariants using `PY4GW_ASSERT`, `PY4GW_REQUIRE`, or `PY4GW_PANIC`?
- Does hook resolution go through project scanning and assertion paths?
- Does the migration avoid adding new behavior beyond documented project rules?
- If behavior changed intentionally, is the deviation documented explicitly?
- If the migrated code depends on legacy shared behavior, was that prerequisite migrated first instead of replaced with a shim?
- Is third-party code isolated from project-owned policy changes?
- Is the module documented if it introduces a new subsystem rule?
- Does the documentation remain free of mojibake and mixed-encoding artifacts?
