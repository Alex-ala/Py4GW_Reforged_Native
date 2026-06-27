# ImGui Ini Persistence

## Scope

This note captures the current ImGui window persistence behavior observed in `C:\Users\Apo\Py4GW_Reforged`, why it breaks down for multi-client injection, and which implementation paths make sense next.

## Current Findings

### 1. ImGui default ini persistence is currently disabled

In `C:\Users\Apo\Py4GW_Reforged\src\imgui\imgui_manager.cpp`, ImGui is initialized with:

```cpp
io.IniFilename = nullptr;
```

That means Dear ImGui is not automatically reading or writing an `.ini` file for window positions, sizes, docking layout, or collapsed state.

Implication:

- There is no active shared-file collision in the current checked code path.
- If persistence is observed elsewhere, it is either:
  - from a different branch/build,
  - from custom save/load code outside this file,
  - or from a later/local modification that re-enabled `IniFilename`.

### 2. ImGui persistence is single-target per context

Dear ImGui persistence is based on one ini target per `ImGuiContext`.

If multiple Guild Wars clients inject the same DLL and all contexts point at the same ini file:

- each client loads the same layout state,
- each client overwrites the same file,
- the last client to save wins,
- window coordinates and docking state converge to a single shared layout,
- clients effectively fight over persistence.

This is expected behavior, not a bug in ImGui.

### 3. `Py4GW_Reforged` already resolves assets relative to the injected module

The project already treats the DLL directory as the runtime root for content such as:

- `fonts/`
- `scripts/`
- `offsets/`

This pattern is visible through `process_manager::GetModuleDirectory()` usage in the codebase, including:

- font discovery
- pattern loading
- Python script path setup
- logging outputs

That makes the DLL directory a reasonable place to store per-client ImGui ini files as well, as long as filenames are unique per client.

## Problem Statement

The actual problem is not "how to make one ini file safe for multiple clients."

The real problem is:

- multiple injected client instances need persistence,
- but they must not share the same persistence file unless the shared layout is intentional.

There is no ImGui mode that makes a single shared ini file behave as independent per-client storage.

## What ImGui Supports

ImGui gives a few practical control points:

- `io.IniFilename = nullptr`
  - disables built-in file persistence entirely.
- `io.IniFilename = "<path>"`
  - enables built-in persistence to a specific file.
- `ImGui::LoadIniSettingsFromDisk(...)`
  - explicit load from a chosen file.
- `ImGui::SaveIniSettingsToDisk(...)`
  - explicit save to a chosen file.
- `ImGui::SaveIniSettingsToMemory()` / `LoadIniSettingsFromMemory(...)`
  - allows a custom persistence backend if file I/O should be controlled manually.

These APIs solve file selection and persistence routing. They do not provide multi-client identity management for you.

## Recommended Direction

### Preferred recommendation: one ini file per client identity

The correct default design for multi-client injection is:

- one ImGui context per injected process,
- one unique ini file per client,
- stored beside the DLL or in another predictable runtime-owned directory.

This preserves:

- independent window coordinates,
- independent docking state,
- independent collapsed/open state,
- no cross-client overwrite race.

## Candidate Naming Strategies

### Option A: per-process file

Example:

- `imgui_1234.ini`
- where `1234` is `GetCurrentProcessId()`

Pros:

- trivial to implement,
- immediately stops active collisions,
- no need for game-specific identity lookup.

Cons:

- not stable across restarts,
- every relaunch produces a different file,
- stale ini files accumulate over time.

Use this when:

- the immediate goal is "stop clients fighting each other now."

### Option B: per-character or per-account file

Example:

- `imgui_accountname.ini`
- `imgui_charactername.ini`
- `imgui_account_character.ini`

Pros:

- stable across restarts,
- one logical client profile keeps its own layout,
- much better operational behavior than PID naming.

Cons:

- requires access to a stable identity source,
- requires sanitizing file names,
- needs clear fallback behavior when identity is unavailable early in startup.

Use this when:

- the project can reliably resolve account, character, or launch-profile identity.

### Option C: per-launch-profile file

Example:

- `imgui_profile_mainfarm.ini`
- `imgui_profile_alt2.ini`

Pros:

- stable,
- human-readable,
- aligns with multibox operational workflows.

Cons:

- requires explicit configuration or launcher support,
- adds a small amount of runtime config surface.

Use this when:

- clients are intentionally launched under named roles or profiles.

### Option D: custom memory or centralized persistence backend

Store ImGui settings in memory, JSON, or another application-defined format and explicitly call ImGui load/save helpers.

Pros:

- maximum control,
- can merge with existing script/config systems,
- can support profile selection, export/import, or version migration.

Cons:

- more code,
- more failure modes,
- unnecessary unless the project already needs a broader settings system.

Use this when:

- persistence needs to be part of a larger project configuration architecture.

## Practical Recommendation Order

### Short term

Implement per-process ini naming first if the immediate goal is to stop collisions with minimal code churn.

This is the fastest safe step.

### Medium term

Move to a stable per-client identity filename when a reliable identity source exists.

This is the best long-term default.

### Long term

Only build a custom persistence backend if there is a broader requirement for:

- versioned settings,
- profile switching,
- centralized config,
- or synchronization with Python-side tooling.

## Suggested Implementation Shape

Inside ImGui initialization, replace:

```cpp
io.IniFilename = nullptr;
```

with logic that computes a unique path and stores it in a static string with lifetime long enough for ImGui to reference safely.

Example shape:

```cpp
static std::string ini_path =
    (py4gw::process_manager::GetModuleDirectory() /
     ("imgui_" + std::to_string(::GetCurrentProcessId()) + ".ini")).string();
io.IniFilename = ini_path.c_str();
```

Important details:

- Do not assign `io.IniFilename` to a temporary string buffer.
- The backing storage must outlive the ImGui context.
- If using account or character names, sanitize characters invalid in Windows file names.
- If identity is unavailable at first initialization, choose either:
  - a fallback filename,
  - delayed persistence activation,
  - or a later migration/rename step.

## Risks And Tradeoffs

### If persistence remains disabled

Pros:

- no collisions,
- no file management complexity.

Cons:

- no saved positions,
- no saved docking layout,
- poor usability for repeated sessions.

### If one shared ini file is used

Pros:

- simple,
- one canonical layout.

Cons:

- clients overwrite each other,
- impossible to preserve independent layouts,
- not suitable for multi-client usage unless that shared layout is explicitly desired.

### If per-process ini is used

Pros:

- no active cross-client collision.

Cons:

- no stable persistence per logical client across restarts.

### If stable per-client ini is used

Pros:

- best match for multibox workflow,
- independent and durable layouts.

Cons:

- requires identity resolution and sanitization.

## Final Recommendation

For `Py4GW_Reforged`, the best path is:

1. Re-enable ImGui persistence deliberately rather than implicitly.
2. Use a unique ini file per injected client.
3. Start with PID-based naming if a fast fix is needed.
4. Upgrade to account/character/profile-based naming once a stable client identity is available.

Unless a shared layout is intentionally wanted across all clients, a single shared `imgui.ini` should be avoided.
