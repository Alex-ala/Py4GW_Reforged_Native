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

## Review Checklist

- Does the header include `base/error_handling.h`?
- Are fatal invariants using `PY4GW_ASSERT`, `PY4GW_REQUIRE`, or `PY4GW_PANIC`?
- Does hook resolution go through project scanning and assertion paths?
- Is third-party code isolated from project-owned policy changes?
- Is the module documented if it introduces a new subsystem rule?
