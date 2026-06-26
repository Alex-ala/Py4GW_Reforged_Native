# Current Observations And Risks

This file documents the current state observed during documentation review. It is not a design proposal. It records facts and likely maintenance risks.

## 1. `offsets/render.json` Is Not Strict JSON

The current file ends with a trailing comma after the `patterns` object.

Example shape observed:

```json
{
  "namespace": "render",
  "patterns": {
    ...
  },
}
```

For `nlohmann::json`, this should be treated as invalid JSON and likely to fail at runtime when `Patterns::Initialize()` loads it.

## 2. Pattern `offset` Semantics Are Not Uniform

The project deliberately chose a thin loader, which is reasonable, but the current consumers interpret `offset` differently:

- `window_handle_ptr`
  used as the displacement passed to `Scanner::Find`
- `reset_target`
  used as the `ToFunctionStart` scan range
- `end_scene_target`
  used as the displacement passed to `Scanner::Find`

This is workable as long as call sites stay explicit, but it means a JSON reader cannot infer semantics from the field alone.

## 3. `end_scene_target` Is Only Partially Data-Driven

`render.cpp` now mirrors the original RenderMgr flow more closely: `reset_target` uses the loaded `offset` as the `ToFunctionStart` scan range, while `end_scene_target` resolves its scan hit first and then walks back to the function start with the scanner default range.

That asymmetry should be remembered when expanding the pattern set.

## 4. Scanner Assumptions Are Strongly 32-Bit And Legacy-Prologue Based

The scanner depends on:

- `IMAGE_FILE_MACHINE_I386`
- 4-byte address references
- `55 8B EC` style function-start discovery

That is valid for the target, but it is brittle if the executable build style changes.

## 5. ImGui Input Path Is A Complexity Hotspot

The WndProc code is the most behavior-dense part of the repo.

Risks:

- subtle input regressions
- drag-state bugs
- game input capture conflicts
- difficult-to-debug message ordering issues

Any changes there should be tested in the live target, not just compiled.

## 6. Shutdown Is Careful But Still Timing-Sensitive

Good parts:

- render callbacks are detached before subsystem teardown
- hook calls are waited out with `g_in_hook_count`
- runtime thread sleeps before unloading

Still, this is injected code inside a live game process. Device callbacks, WndProc interception, and Python teardown remain timing-sensitive even with the current safeguards.

## 7. The Update Loop Is Still A Stub

`UpdateLoopStep()` currently only sleeps.

That is fine for now, but it means there is not yet a formal home for:

- script ticking
- scheduled tasks
- message dispatch
- plugin lifecycle coordination

## 8. Generated Build Artifacts Contain Historical Leftovers

The `build/` directory includes object names for earlier experimental files such as `offset_store.obj`.

That is not a source problem by itself, but it is a reminder that `build/` should not be used as documentation for current architecture.

