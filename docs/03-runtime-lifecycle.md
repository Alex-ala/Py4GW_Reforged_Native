# Runtime Lifecycle

## Main Entry Points

The exported C API is declared in `include/Py4GW.h`:

- `Py4GW_Initialize()`
- `Py4GW_Shutdown()`
- `Py4GW_RequestShutdown()`

It also declares:

- `py4gw::RuntimeThread(LPVOID)`

## DLL Attachment

`src/dllmain.cpp` is intentionally minimal:

1. On `DLL_PROCESS_ATTACH`
   - disables thread library notifications
   - stores the module handle in `process_manager`
   - creates a dedicated runtime thread
2. On `DLL_PROCESS_DETACH`
   - calls `Py4GW_Shutdown()` only when `reserved == nullptr`

This keeps heavy work out of `DllMain`, which is the correct shape for an injected DLL.

## Runtime Ownership Model

`src/Py4GW.cpp` owns the runtime state machine.

Internal globals:

- `g_runtime_mutex`
- `g_running`
- `g_shutdown_requested`

The mutex serializes initialize/shutdown transitions. There is no broader fine-grained subsystem lock hierarchy here; lifecycle is treated as a coarse, process-wide state transition.

## Initialization Sequence

`Py4GW_Initialize()` does the following in order:

1. Acquire the runtime mutex
2. Return early if already running
3. Configure the logger output path
4. Clear the shutdown-request flag
5. Initialize the embedded Python runtime
6. Initialize the Guild Wars render hook layer
7. Register ImGui shutdown callback
8. Register Guild Wars render/reset callbacks
9. Mark the runtime as running
10. Log success

Failure handling:

- Python init failure aborts immediately
- Guild Wars init failure logs and triggers Python shutdown rollback

## Steady-State Runtime Loop

`RuntimeThread()` calls `Py4GW_Initialize()` and then loops until shutdown is requested.

Per-iteration behavior is currently minimal:

- `UpdateLoopStep()` only sleeps for 10 ms

This function is explicitly reserved for future responsibilities:

- Python update callbacks
- timers
- process polling
- future game-thread related work

## Render Callback Flow

The hook layer invokes:

- `DrawLoop(IDirect3DDevice9* device)`
- `OnReset(IDirect3DDevice9* device)`

`DrawLoop` checks `g_running` and delegates to `imgui_manager::Render`.

`OnReset` calls:

- `imgui_manager::InvalidateDeviceObjects()`

## Shutdown Request Path

Shutdown can be triggered by:

- explicit external call to `Py4GW_RequestShutdown()`
- ImGui shutdown button via callback

`BeginShutdown()` only sets a flag. Real teardown is deferred to the runtime thread.

## Shutdown Sequence

`Py4GW_Shutdown()` does the following:

1. Acquire runtime mutex
2. Return early if not running
3. Remove render/reset callbacks from the GW layer
4. Shut down ImGui manager
5. Shut down Guild Wars hook layer
6. Shut down Python runtime
7. Clear running and shutdown-request flags
8. Log completion

This order is reasonable:

- stop frame callbacks first
- tear down overlay next
- unhook render targets
- destroy Python last among active subsystems

## Thread Exit

After shutdown is requested, `RuntimeThread()`:

1. sleeps 50 ms to let the requesting frame unwind
2. calls `Py4GW_Shutdown()`
3. unloads the module with `FreeLibraryAndExitThread()`

That final unload depends on `process_manager::GetModuleHandle()` having been set during process attach.

## Lifecycle Constraints

Current lifecycle assumptions:

- single injected runtime per process
- no restart while partially initialized
- callbacks are only valid while `g_running == true`
- the runtime thread is the owner of the final unload

## Notable Design Tradeoffs

### Good

- Heavy initialization is outside `DllMain`
- Initialization rollback is at least partially present
- Shutdown is centralized

### Thin Areas

- The update loop is still a placeholder
- There is no state enum, only booleans
- Partial initialization states are not deeply modeled
- No explicit diagnostics are exposed to tell which subsystem failed besides logs
