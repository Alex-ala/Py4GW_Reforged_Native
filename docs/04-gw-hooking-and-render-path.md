# Guild Wars Hooking And Render Path

## Purpose

The `gw` subsystem is responsible for:

- locating runtime addresses inside Guild Wars
- resolving the game window handle storage
- locating the DX9 render/reset functions
- installing hooks with MinHook
- forwarding render/reset events into project callbacks

Public interface:

- `gw::Initialize()`
- `gw::Shutdown()`
- `gw::GetWindowHandle()`
- `gw::SetRenderCallback()`
- `gw::SetResetCallback()`

Defined in:

- `include/GW/render/render.h`
- `src/GW/render/render.cpp`

## Internal State

Important globals in `src/GW/render/render.cpp`:

- `g_window_handle_ptr`
- `g_end_scene_target`
- `g_end_scene_original`
- `g_reset_target`
- `g_reset_original`
- `g_render_callback`
- `g_reset_callback`
- `g_render_lock`
- `g_initialized`
- `g_in_hook_count`

These cover:

- resolved addresses
- original trampoline targets
- user callback storage
- synchronization and shutdown coordination

## Pattern-Backed Address Discovery

The hook layer currently pulls binary patterns from `Patterns::Get(...)`.

Pattern names used here:

- `render.window_handle_ptr`
- `render.reset_target`
- `render.end_scene_target`

The meaning of each loaded pattern is still determined by `render.cpp`, not by the pattern loader.

### `ResolveWindowHandlePointer()`

Flow:

1. fetch pattern object
2. call `Scanner::Find(pattern, mask, offset, section)`
3. dereference the returned address as a pointer-sized value
4. validate that pointer against the `.data` section
5. store it in `g_window_handle_ptr`

The important point is that the pattern object supplies raw scan inputs, but the dereference and validation remain hand-written logic.

### `ResolveRenderTargets()`

Flow:

1. fetch `render.reset_target`
2. fetch `render.end_scene_target`
3. run `Scanner::Find(...)` for both
4. call `Scanner::ToFunctionStart(...)`
5. cast the results to `ResetFn` and `EndSceneFn`

Current asymmetry:

- `reset_target.offset` is used as the `ToFunctionStart` scan range
- `end_scene_target.offset` is passed into `Scanner::Find`
- `end_scene_target` still uses a hardcoded `0x0FFF` function-start scan range

This is a real current implementation detail and should be documented because it affects how future JSON entries should be interpreted.

## Hook Installation

`InstallHook(void** target, void* detour, void** trampoline)` does three things:

1. validates that the target pointer exists
2. optionally resolves a nested target through `Scanner::FunctionFromNearCall(...)`
3. installs and enables the MinHook detour

That extra `FunctionFromNearCall` step is important because some resolved addresses may point at small jump/call stubs rather than the final body.

## Hook Removal

`RemoveHook(void* target)` simply disables and removes a MinHook target when non-null.

## Hook Callback Behavior

### `OnEndScene`

- increments in-hook counter
- enters `g_render_lock`
- invokes registered render callback with the D3D9 device
- calls the original EndScene
- leaves the critical section
- decrements in-hook counter

This lock is the primary synchronization mechanism around the render callback path.

### `OnReset`

- increments in-hook counter
- invokes registered reset callback
- calls the original Reset
- decrements in-hook counter

Unlike `OnEndScene`, `OnReset` does not take `g_render_lock`.

## Initialization Sequence

`gw::Initialize()` does:

1. early return if already initialized
2. initialize scanner
3. initialize pattern loader
4. resolve window handle pointer
5. resolve render targets
6. initialize critical section
7. initialize MinHook
8. install EndScene and Reset hooks
9. swap final target pointers to the nested/adjusted versions
10. mark initialized

## Shutdown Sequence

`gw::Shutdown()` does:

1. early return if not initialized
2. remove both hooks
3. wait up to roughly 160 ms for hook calls to drain
4. uninitialize MinHook
5. delete critical section
6. clear callback pointers and resolved addresses
7. mark uninitialized

The in-hook wait loop reduces the chance of tearing down while callbacks are still active.

## Window Handle Exposure

`gw::GetWindowHandle()` reads the actual HWND value by dereferencing `g_window_handle_ptr`.

That means:

- `g_window_handle_ptr` is not the `HWND`
- it is the address of a game-owned storage slot containing the `HWND`

## Render Context Structure

`GwDxContext` is a partial reverse-engineered structure used only to reach:

- `IDirect3DDevice9* device`
- viewport and window dimension fields

Most of its fields are anonymous padding placeholders.

This structure is a maintenance hotspot because any game update that changes layout may invalidate offsets into it.


