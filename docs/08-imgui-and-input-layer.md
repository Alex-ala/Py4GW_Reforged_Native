# ImGui And Input Layer

## Purpose

The ImGui manager owns:

- lazy ImGui initialization on the game D3D9 device
- font catalog setup and Font Awesome merge support
- integration with ImGui DX9 and Win32 backends
- subclassing the Guild Wars window procedure
- deciding when to capture or forward input
- issuing a simple overlay UI

Files:

- `include/imgui/imgui_manager.h`
- `include/imgui/font_manager.h`
- `src/imgui/imgui_manager.cpp`
- `src/imgui/font_manager.cpp`

## Initialization Strategy

ImGui does not initialize during project bootstrap. It initializes lazily inside `Render(device)` on first valid use.

This is practical because:

- the project only gets a reliable `IDirect3DDevice9*` through the render hook
- device readiness can be verified at render time

## Font Stack

The font layer is centralized in `imgui::FontManager`.

Responsibilities:

- resolve the `fonts/` directory relative to the injected module or target process
- load the Friz Quadrata font family in project-defined sizes
- merge Font Awesome glyphs into the atlas
- expose stable font IDs instead of scattering `AddFontFromFileTTF()` calls

`imgui_manager` now only triggers font initialization through `SetupImGuiFonts()`. It no longer owns font path discovery or direct font file loading.

## Window Procedure Subclassing

The code replaces the game window’s WndProc with `SafeWndProc`, which wraps `WndProc` in a SEH guard.

Stored state:

- `g_game_window`
- `g_original_wndproc`
- `g_wndproc_attached`

`AttachWndProc()`:

1. gets the game window via `gw::GetWindowHandle()`
2. installs a new window proc with `SetWindowLongPtrW`
3. registers raw mouse input sink
4. marks the custom WndProc as attached

`RestoreWndProc()` reverts that subclassing.

## Why `SafeWndProc` Exists

Injected code running inside a game window proc is a crash-risk hotspot. The SEH wrapper reduces the chance that an exception in input handling kills the host process immediately.

## Input Handling Behavior

The custom WndProc tracks cached mouse state:

- left button
- right button
- mouse coordinates

It then updates `ImGuiIO` manually before deciding whether to:

- feed messages into ImGui
- swallow them
- pass them to the original window proc

## Dragging Logic

The input layer has a special dragging model:

- `g_is_dragging`
- `g_is_dragging_imgui`
- `g_dragging_initialized`

The intent is to distinguish:

- dragging initiated over ImGui
- dragging initiated outside ImGui

This avoids some common overlay bugs where a drag begun in the game world gets stolen halfway through by the overlay, or vice versa.

## Right Mouse Pass-Through

When right mouse is held:

- the code preserves cached mouse button state
- forwards to the original WndProc

This is likely intended to preserve in-game camera/control behavior while still keeping ImGui’s internal mouse state updated.

## Message Capture Rules

The WndProc swallows messages when ImGui wants them, primarily for:

- mouse movement and mouse buttons
- mouse wheel
- keyboard/text input

It still explicitly passes a few window-text related messages to `DefWindowProcW`:

- `WM_SETTEXT`
- `WM_GETTEXT`
- `WM_GETTEXTLENGTH`

That reduces the chance of breaking title/text related system behavior.

## Render Flow

`imgui_manager::Render(device)`:

1. lazy-initializes ImGui if needed
2. checks `device->TestCooperativeLevel()`
3. invalidates DX9 device objects if the device is not cooperative
4. starts Win32 and DX9 new-frame setup
5. builds a small overlay window
6. renders draw data
7. restores a few D3D9 render states

Current UI contents are intentionally simple:

- runtime status bullets
- shutdown button

## Device Reset Handling

`InvalidateDeviceObjects()` calls:

- `ImGui_ImplDX9_InvalidateDeviceObjects()`

This is wired to the Guild Wars reset hook so ImGui resources are not left stale across DX9 device resets.

## Shutdown Path

`imgui_manager::Shutdown()`:

1. restores the original WndProc
2. shuts down ImGui DX9 backend
3. shuts down ImGui Win32 backend
4. destroys the ImGui context
5. clears initialization state

## Design Notes

### Strengths

- lazy init avoids device-availability issues
- explicit WndProc restore is present
- SEH guard is pragmatic for injected code

### Complexity Hotspot

The WndProc input path is the densest and riskiest part of the repo. Any future behavior changes here should be treated cautiously because they can affect:

- game input
- overlay capture
- drag behavior
- shutdown usability
