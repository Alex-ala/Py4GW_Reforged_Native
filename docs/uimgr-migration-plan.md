# UIManager Migration Plan

## Goal

Migrate the legacy native `UIManager` from `C:\Users\Apo\Py4GW` into `Py4GW_Reforged` as a split native `gw::ui` module.

Scope rules:

- migrate native `uimgr` functionality only
- do not migrate `py_ui.cpp`, `pybind11`, or any Python-shaped accessor surface
- `*_methods.h/.cpp` are for callable methods only
- hook ownership, detours, enable or disable flow, and teardown stay in `ui.cpp` or another dedicated non-`methods` file
- if a native legacy function cannot be ported safely now, copy the native logic and leave it commented with a short blocker note

This plan follows [docs/module-migration-guide.md](/C:/Users/Apo/Py4GW_Reforged/docs/module-migration-guide.md:1).

## Execution Status

This migration is already in progress. The repo is not at the planning-only stage anymore.

Completed execution:

- native `gw::ui` module created
- build integration added to `CMakeLists.txt`
- lifecycle integration added to [src/GW/GuildWars.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/GuildWars.cpp:1)
- no Python-side `ui` migration was added
- hooks remain owned by `src/GW/ui/ui.cpp`
- methods were split into focused native files instead of recreating a monolith
- the public UI surface is now regrouped by responsibility so types, globals, callbacks, hooks, and callable methods are easier to audit

Current migrated files:

- [include/GW/ui/ui.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui.h:1)
- [include/GW/ui/ui_types.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_types.h:1)
- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [include/GW/ui/ui_callbacks.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_callbacks.h:1)
- [include/GW/ui/ui_module.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_module.h:1)
- [include/GW/ui/ui_hooks.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_hooks.h:1)
- [include/GW/ui/ui_core.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_core.h:1)
- [include/GW/ui/ui_frame_methods.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_frame_methods.h:1)
- [include/GW/ui/ui_control_methods.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_control_methods.h:1)
- [include/GW/ui/ui_deferred_preferences.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_deferred_preferences.h:1)
- [src/GW/ui/ui.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui.cpp:1)
- [src/GW/ui/ui_callbacks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_callbacks.cpp:1)
- [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)
- [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)
- [src/GW/ui/ui_hooks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_hooks.cpp:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- [src/GW/ui/ui_deferred_preferences.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_deferred_preferences.cpp:1)
- [offsets/ui.json](/C:/Users/Apo/Py4GW_Reforged/offsets/ui.json:1)

## Current Split

The current split is:

- `ui_types.h`
  - frame structs
  - packet structs
  - enums
- `ui_symbols.*`
  - callable function-pointer typedefs
  - resolved native pointers
  - native address globals
  - callback lock and lifecycle state
- `ui_callbacks.*`
  - UI message callback registration
  - frame UI message callback registration
  - create-component callback registration
  - callback dispatch and state clearing
- `ui_module.h`
  - public module lifecycle declarations
- `ui_hooks.*`
  - preserved hook-driven parity that is intentionally not part of callable APIs yet
- `ui.cpp`
  - module lifecycle
  - scanner and resolver ownership
  - detour ownership
  - hook enable and disable flow
  - shutdown draining and callback-state clearing
- `ui_frame_methods.*`
  - frame lookup
  - hierarchy traversal
  - frame state reads and writes
  - title and geometry reads that are callable methods
  - UI global state reads such as world map, screenshot, and draw state
  - window position reads
  - window visibility and position writes
  - compass draw helper
  - settings load and settings blob access
  - tooltip access
  - audio volume helpers
- `ui_control_methods.*`
  - raw UI dispatch
  - frame-targeted UI dispatch
  - component creation and destruction methods
  - redraw and interaction helpers
  - key-action helpers
  - typed control-creation wrappers
  - scrollable item management
  - scrollable iteration and selection helpers
  - label and encoded-text helpers
  - control value getters and setters
  - commented native parity for unresolved flat-button subclass behavior
  - commented native parity for unresolved async decode behavior

This is the correct direction for the migration. We are not using `*_methods` files as hook containers.

This layout is intended to answer the navigation problem directly:

- types are grouped with types
- globals are grouped with globals
- callbacks are grouped with callbacks
- hooks and lifecycle are grouped with hooks and lifecycle
- callable APIs stay in the method-family files
- callable APIs are now collapsed into two real method families instead of many thin slices

Deferred hook-driven parity is now preserved explicitly in:

- `ui_hooks.*`
- `ui_deferred_preferences.*`

## What Is Already Migrated

### Phase 1 Complete: Core Native Module

Completed:

- `Initialize()` and `Shutdown()`
- frame array resolution
- root frame resolution
- frame lookup by id, hash, and label
- parent and child traversal helpers
- related-frame helpers
- frame visibility, disabled state, opacity, and layer methods
- title reads and frame state reads
- frame clip, position, native size, and context reads
- raw UI message dispatch
- frame-targeted UI message dispatch
- UI message callback registration
- frame UI message callback registration
- `IsUIDrawn()`
- `IsWorldMapShowing()`
- `IsShiftScreenShot()`
- frame inspection helpers:
  - hash by label
  - frame hierarchy
  - frame coords by hash

### Phase 2 Partially Complete: Native Types And Resolvers

Completed:

- core frame structs required by the current native surface
- frame relation and frame position modeling
- function pointer typedefs for migrated callable methods
- JSON-backed scanner entries in [offsets/ui.json](/C:/Users/Apo/Py4GW_Reforged/offsets/ui.json:1)
- resolver ownership centralized in `ui.cpp`

Still expected:

- more native structs for additional window and control helpers
- more resolver entries for settings, compass, and window-position functionality
- more commented parity blocks where a legacy native function cannot yet be validated

### Phase 3 Complete: Module Integration

Completed:

- UI sources added to [CMakeLists.txt](/C:/Users/Apo/Py4GW_Reforged/CMakeLists.txt:1)
- module startup wired into [src/GW/GuildWars.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/GuildWars.cpp:1)
- module shutdown wired into `GuildWars.cpp`
- startup order currently places `ui` after `render`

## Remaining Migration Batches

The remaining work should continue in controlled native batches.

### Batch A Complete: Window And Settings Methods

Completed:

- window position access
- window visibility writes
- window position writes
- compass draw helper
- UI settings load helper
- settings blob access
- current tooltip access
- volume setter
- master volume setter

Implemented in:

- `include/GW/ui/ui_frame_methods.h`
- `src/GW/ui/ui_frame_methods.cpp`

Constraints preserved:

- callable methods only
- no Python `WindowID` surface
- resolver ownership kept in `ui.cpp`

### Batch B Partially Complete: Higher-Risk Component Mutation

Completed:

- keydown, keyup, and keypress helpers
- button click helper
- dropdown selection helper
- frame margins mutation helper
- typed control creation for:
  - button
  - engine button
  - text button
  - checkbox
  - scrollable
  - text label
  - dropdown
  - slider
  - editable text
  - progress bar
  - tabs
- text and value helpers for:
  - button labels
  - text labels
  - multi-line text labels
  - editable text
  - progress bars
  - checkboxes
  - dropdown options, indices, and selected value
  - slider value reads and writes
  - encoded-string validation
  - uint32 to encoded-string conversion
  - encoded-string to uint32 conversion
- scrollable helpers for:
  - sort handler get and set
  - add, remove, clear, and enumerate items
  - selected value reads
  - child iteration
  - page get and set
- tabs message helpers for:
  - add tab
  - enable tab
  - disable tab
  - remove tab
  - get current tab index
  - get tab frame id
  - get tab enabled state
  - get tab by label
  - get current tab
  - choose tab by frame
  - choose tab by index
  - get tab button
- manager-owned resolution for typed component callbacks
- manager-owned resolution for frame subclass attachment
- commented native parity preserved for flat-button click behavior blocked on unresolved subclass data

Still expected in this batch:

- create window by frame id variants
- destroy by frame id variants
- clear children or clear content helpers
- clone-style helpers
- any extra payload structs these reveal

### Batch C: Specialized Controls

- buttons
- checkboxes
- scrollables
- labels
- dropdowns
- sliders
- editable text
- progress bars
- tabs

Only migrate these after generic frame and message paths are stable.

### Batch D: Hook-Driven Legacy UI Features

- title-related detours
- dialog-title behavior
- nonclient or special frame hooks

These should remain isolated from callable methods.

## Missing-Dependency Policy

When a legacy native `UIManager` function depends on missing support:

- if the dependency is UI-owned, add it under `GW/ui`
- if it is shared infrastructure used by multiple modules, split it out deliberately
- if it only exists to shape data for Python, do not migrate it now

Expected missing dependencies:

- additional frame fields
- window-position array typing
- game settings blob structures
- encoded-string and text helper details
- control-specific payload layouts
- title-resource creation details

## Commented-Parity Rule

If a native legacy function cannot be migrated safely:

1. copy the native legacy logic into the correct new file
2. leave it commented out
3. add a short note describing the blocker

Use this when the blocker is:

- missing struct layout
- unresolved scanner or callsite
- crash risk from hook behavior
- correctness risk from partial reverse engineering

Current commented native parity example:

- flat-button click creation path is copied but left commented because the dialog subclass data address is still unresolved
- async decode logic is copied but left commented because text parser context and decode callback lifecycle are not migrated yet
- deferred tooltip and async decode hook lifecycle is preserved in `ui_hooks.cpp`
- deferred preference and text-language subsystem parity is preserved in `ui_deferred_preferences.cpp`

Do not drop native parity logic silently.

## Explicit Deferrals

Still out of scope:

- `py_ui.cpp`
- `pybind11` bindings
- Python-side accessors
- `py::tuple` return shaping
- Python API naming compatibility

If a method exists only for Python-facing convenience, do not port it in this migration.

## Remaining Follow-Up

The core native `uimgr` migration is already executed. Remaining follow-up is limited to completion hardening:

1. keep filling missing native-only helpers only when another native module needs them
2. move any newly blocked legacy-native paths into commented parity blocks instead of dropping them
3. migrate additional UI-owned dependencies under `GW/ui` if later native callers expose a gap
4. keep hook ownership in `ui.cpp` or another dedicated non-`methods` file
5. rebuild after each follow-up slice to catch missing struct or resolver dependencies early

## Definition Of Success

This migration is successful when:

- the native `gw::ui` module covers the required legacy `uimgr` surface without dragging Python wrappers with it
- the module remains split into callable-method files plus manager-owned hook lifecycle
- remaining unsupported native functions are preserved as commented parity blocks instead of being discarded
- downstream native systems can depend on `gw::ui` without using the legacy monolith
