# UIManager Migration Map

## Purpose

This is the crosswalk from legacy `C:\Users\Apo\Py4GW\vendor\gwca\Source\UIMgr.cpp` to the split native `gw::ui` module in `Py4GW_Reforged`.

Use this document when you find a legacy `typedef`, `*_Func`, `*_Ret`, or helper in `UIMgr.cpp` and need to know where it ended up.

## Core Rule

The migration follows this split:

- legacy function pointer typedefs moved to [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1) as `using ...Fn`
- legacy global function pointer declarations moved to [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- legacy global function pointer storage moved to [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1) as `g_...`
- callback registration and dispatch now live together in [include/GW/ui/ui_callbacks.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_callbacks.h:1) and [src/GW/ui/ui_callbacks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_callbacks.cpp:1)
- scanner resolution and hook ownership stayed in [src/GW/ui/ui.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui.cpp:1098)
- callable native wrappers moved to the focused `*_methods.cpp` files under [src/GW/ui](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui)
- blocked native-only parity that could not be finished safely was copied and left commented in `ui_deferred_*` or the relevant methods file

## Naming Conversion

Legacy names were normalized instead of copied 1:1.

- `SomeType_pt` became `SomeTypeFn`
- `SomeType_Func` became `g_some_type_func`
- `RetSomeType` or `SomeType_Ret` became `g_some_type_original` when the pointer is the post-hook original target
- raw address globals such as `Thing_Addr` became `g_thing_addr`
- native callable methods became `gw::ui::MethodName(...)`

## Example: SendUIMessage

Legacy:

```cpp
typedef void (__cdecl *SendUIMessage_pt)(UI::UIMessage msgid, void *wParam, void *lParam);
SendUIMessage_pt SendUIMessage_Func = 0;
SendUIMessage_pt RetSendUIMessage = 0;
```

Reforged mapping:

- typedef: [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
  `using SendUIMessageFn = void(__cdecl*)(UIMessage message_id, void* wparam, void* lparam);`
- active pointer declaration: [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
  `extern SendUIMessageFn g_send_ui_message_func;`
- original pointer declaration: [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
  `extern SendUIMessageFn g_send_ui_message_original;`
- storage definitions: [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- resolver and hook creation: [src/GW/ui/ui.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui.cpp:709)
- public callable wrapper: [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)

So the direct crosswalk is:

- `SendUIMessage_pt` -> `SendUIMessageFn`
- `SendUIMessage_Func` -> `g_send_ui_message_func`
- `RetSendUIMessage` -> `g_send_ui_message_original`

## Function Pointer Crosswalk

### Messaging

- `SendUIMessage_pt` -> `SendUIMessageFn`
- `SendUIMessage_Func` -> `g_send_ui_message_func`
- `RetSendUIMessage` -> `g_send_ui_message_original`
- `SendFrameUIMessage_pt` -> `SendFrameUIMessageFn`
- `SendFrameUIMessage_Func` -> `g_send_frame_ui_message_func`
- `SendFrameUIMessage_Ret` -> `g_send_frame_ui_message_original`
- `SendFrameUIMessageById_pt` -> `SendFrameUIMessageByIdFn`
- `SendFrameUIMessageById_Func` -> `g_send_frame_ui_message_by_id_func`
- `SendFrameUIMessageById_Ret` -> `g_send_frame_ui_message_by_id_original`

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- wrappers: [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)

### Frame Lookup

- `GetChildFrameId_pt` -> `GetChildFrameIdFn`
- `GetChildFrameId_Func` -> `g_get_child_frame_id_func`
- `FindRelatedFrame_pt` -> `FindRelatedFrameFn`
- `FindRelatedFrame_Func` -> `g_find_related_frame_func`
- `GetRootFrame` legacy resolver target -> `GetRootFrameFn`
- root function pointer -> `g_get_root_frame_func`
- `GetChildFromNameHash_pt` has no dedicated stored function pointer in `ui_core.h`; the behavior is implemented through frame traversal helpers and migrated callable methods in [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)

### Hashing and Component Creation

- `CreateHashFromWchar_pt` -> `CreateHashFromWcharFn`
- `CreateHashFromWchar_Func` -> `g_create_hash_from_wchar_func`
- `CreateHashFromWchar_Ret` was not kept as a separate `original` global because the migrated surface uses the resolved callable directly rather than a hook-owned original
- `CreateUIComponent_pt` -> `CreateUIComponentFn`
- `CreateUIComponent_Func` -> `g_create_ui_component_func`
- `CreateUIComponent_Ret` -> `g_create_ui_component_original`
- `DestroyUIComponent_pt` -> `DestroyUIComponentFn`
- `DestroyUIComponent_Func` -> `g_destroy_ui_component_func`
- `FrameNewSubclass_pt` -> `FrameNewSubclassFn`
- `FrameNewSubclass_Func` -> `g_frame_new_subclass_func`

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- wrappers: [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)
- control construction: [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)

### Window, Tooltip, Audio, Compass

- `SetTooltip_pt` -> `SetTooltipFn`
- `SetTooltip_Func` -> `g_set_tooltip_func`
- `RetSetTooltip` was not carried as a live migrated original hook target; deferred hook parity is kept in [src/GW/ui/ui_hooks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_hooks.cpp:1)
- `SetWindowVisible_pt` -> `SetWindowVisibleFn`
- `SetWindowVisible_Func` -> `g_set_window_visible_func`
- `SetWindowPosition_pt` -> `SetWindowPositionFn`
- `SetWindowPosition_Func` -> `g_set_window_position_func`
- `SetVolume_pt` -> `SetVolumeFn`
- `SetVolume_Func` -> `g_set_volume_func`
- `SetMasterVolume_pt` -> `SetMasterVolumeFn`
- `SetMasterVolume_Func` -> `g_set_master_volume_func`
- `DrawOnCompass_pt` -> `DrawOnCompassFn`
- `DrawOnCompass_Func` -> `g_draw_on_compass_func`
- `DrawOnCompass_Ret` was not preserved as an active original pointer because the migrated callable surface does not keep that hook path live
- `LoadSettings_pt` -> `LoadSettingsFn`
- `LoadSettings_Func` -> `g_load_settings_func`

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- wrappers: [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)

### Title and Typed Component Internals

- `TitleBinarySearch_pt` -> `TitleBinarySearchFn`
- `TitleBinarySearch_Func` -> `g_title_binary_search_func`
- `TitleTable_Addr` -> `g_title_table_addr`
- `GetTitle_pt` -> `GetTitleFn`
- `GetTitle_Func` -> `g_get_title_func`
- `TypedComponentPassthroughHook_pt` -> `TypedComponentPassthroughFn`
- `TypedComponentPassthroughHook_Func` -> `g_typed_component_passthrough_func`
- `TypedComponentPassthroughHook_Ret` was not kept as a separate active original pointer in the migrated surface

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)
- title/frame readers: [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)

## Legacy Address Globals

Legacy raw addresses also moved into named `g_..._addr` globals:

- `UiFrames_Addr` or `s_FrameArray` -> `g_frame_array`
- `WorldMapState_Addr` -> `g_world_map_state_addr`
- `ui_drawn_addr` -> `g_ui_drawn_addr`
- `shift_screen_addr` -> `g_shift_screen_addr`
- `GameSettings_Addr` -> `g_game_settings_addr`
- `CurrentTooltipPtr` -> `g_current_tooltip_ptr`

Definitions:

- [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- [src/GW/ui/ui_symbols.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_symbols.cpp:1)

## File Ownership Guide

If you are looking for a migrated legacy symbol, use this routing:

- low-level structs, enums, typedefs, extern globals:
  [include/GW/ui/ui_types.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_types.h:1)
  [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
- callback types, callback registration, and callback dispatch:
  [include/GW/ui/ui_callbacks.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_callbacks.h:1)
  [src/GW/ui/ui_callbacks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_callbacks.cpp:1)
- public lifecycle and hook-facing module entry points:
  [include/GW/ui/ui_module.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_module.h:1)
- resolver code, detours, hook lifecycle, global definitions:
  [src/GW/ui/ui.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui.cpp:1)
- frame tree, IDs, hierarchy, state, title, geometry:
  [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)
- frame state, window access, settings, tooltip, audio, compass:
  [src/GW/ui/ui_frame_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_frame_methods.cpp:1)
- send message wrappers, create and destroy component, typed control creation and mutation:
  [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)
- scrollable helpers, text, encoded string, dropdown, slider, checkbox, progress, editable text:
  [src/GW/ui/ui_control_methods.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_control_methods.cpp:1)
- intentionally deferred native-only parity:
  [src/GW/ui/ui_hooks.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_hooks.cpp:1)
  [src/GW/ui/ui_deferred_preferences.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui_deferred_preferences.cpp:1)

## What Was Intentionally Not Migrated

These are not part of the native `uimgr` migration target:

- legacy `py_ui.cpp`
- Python accessor helpers
- `pybind11` binding surfaces
- `py::tuple` shaping helpers

If you find a legacy helper that only exists to expose data to Python, it will usually have no direct `gw::ui` equivalent.

## Practical Lookup Recipe

When you find a symbol in legacy `UIMgr.cpp`:

1. convert `_pt` to `Fn`
2. convert `_Func` to `g_*_func`
3. convert `_Ret` to `g_*_original`
4. check [include/GW/ui/ui_types.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_types.h:1)
5. check [include/GW/ui/ui_symbols.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_symbols.h:1)
6. if it is callback-related, check [include/GW/ui/ui_callbacks.h](/C:/Users/Apo/Py4GW_Reforged/include/GW/ui/ui_callbacks.h:1)
7. check [src/GW/ui/ui.cpp](/C:/Users/Apo/Py4GW_Reforged/src/GW/ui/ui.cpp:1044)
8. then locate the callable wrapper in the relevant `*_methods.cpp`

If you want, the next step can be a second pass that maps whole legacy method blocks from `UIMgr.cpp` to exact new methods one by one.
