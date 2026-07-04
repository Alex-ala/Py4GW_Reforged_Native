# Callback Crash Isolation - Plan (cross-session)

Date opened: 2026-07-04. This is the CURRENT top blocker for the Python migration.
Big task, tackled in chunks. Read this first if injection is crashing.

## The blocker

The console/system consolidation onto PySystem (see
`python-migration-progress.md` -> "System/Console consolidation") fixed the real
import wall (`from Py4GW import Game`). With that gone, `import Py4GWCoreLib` now
completes, the library registers its frame callbacks, and on the first frame a
callback fires with a NULL/garbage callable -> HARD CRASH. Injection dies before it
can even write `error_log.txt` (nothing injects, so no error log is produced -
diagnose from the crash sidecars instead).

Owner is certain this is triggered by recent changes and was not happening before
(before, injection survived because the import failed early at the Game wall, so the
library's callbacks never registered/fired).

## Crash forensics (from F:\GW\GW1\crashes\py4gw-20260704-1456*)

- exception `c0000005`, "Memory at address 00000004 could not be read" (NULL+4 read).
- `.json` context: `{ phase: "runtime", module: "callback" }`.
- pytrace: "No Python frame on the crashing thread."
- stack: python313.dll `PyObject_Vectorcall` (deref of a NULL callable) <- pybind
  `unpacking_collector::call` (cast.h:2254) <- `PyCallback::ExecutePhase`
  (callback.cpp:174, `t->fn()`) <- `DrawLoop` (Py4GW.cpp:304) <- `OnEndScene`.
- So a registered Draw-context callback's stored `pybind11::function fn` is
  NULL/garbage when invoked. This is the SAME signature as the previously reverted
  Vec2 attempt (see `python-migration-restart-handover.md`) - a recurring crash that
  appears whenever the library fully loads and its callbacks run.
- Ruled out: the new `PySystem.get_shared_memory_name` native binding - `Manager::
  Name()` returns `const std::wstring&`, so `std::wstring(Name())` is a safe copy.

Per `never-touch-native-callbacks`: the dispatcher is the messenger, not the bug.
The NULL callable means something REGISTERED a bad callable (None?) or the stored
`py::function` was corrupted/freed earlier. Do NOT "fix" ExecutePhase/register.

## The toggle (owner-requested)

`src/callback/callback.cpp`, just above `PyCallback::ExecutePhase`:

```cpp
static bool g_process_callbacks = false;  // CURRENT: callbacks OFF (surface bring-up)
```

CURRENT STATE (2026-07-04): set to FALSE by owner - callbacks are OFF so injection
survives and we can test the SURFACE (imports). Set true + rebuild only when ready to
attack callbacks. `ExecutePhase` returns immediately when it is false. Plain compile-time bool - NO
Python control (reaching Python requires the surface to already inject). Flip +
rebuild to switch. This is the ONLY sanctioned change to the callback system; do not
add more machinery here.

## Strategy (owner-directed): separate SURFACE from CALLBACKS

1. `g_process_callbacks = false` + rebuild + inject. Confirm the SURFACE comes up:
   `import Py4GWCoreLib` succeeds, widget manager loads, no crash (callbacks are
   registered but never fire).
2. Fix remaining SURFACE issues (non-callback runtime errors that appear once the
   library actually runs - the real "error-by-error" phase, now finally reachable).
   Get the library loading + running cleanly with callbacks OFF.
3. `g_process_callbacks = true` + rebuild. The crash returns -> now attack callbacks.
4. Bisect WHICH callback fires the NULL callable. Tools that already exist (do NOT
   build new ones unless needed): `PyCallback.PyCallback.RemoveByName/PauseById/
   ResumeById`, and `GetCallbackInfo()` to list registered callbacks. Selectively
   pause/resume to find the culprit. Prime suspects (draw/pre-update registrants):
   - `native_src/context/*.py` `_update_ptr` (registered on PreUpdate, context Draw)
   - `native_src/ShMem/SysShaMem.py` `get_payload` (PreUpdate, priority 0)
   - `CombatEvents.py` (Data phase), `TextContext`/string_table enqueue path
   - any widget/module registering on the Draw context
5. Root-cause the bad callable: registered with None, a py::function lifetime/GC
   issue, or memory corruption from a specific Python path (e.g. a ctypes/context
   struct access that writes OOB). Fix at the SOURCE (Python side), never the
   dispatcher.

## Surface fixes log (callbacks OFF, error-by-error via error_log.txt)

- 2026-07-04: `ImportError: cannot import name 'Game'` -> resolved by the PySystem
  consolidation.
- 2026-07-04: `AttributeError: module 'PyInventory' has no attribute 'Bag'` at
  ItemCache.py:150 - the `-> List[PyInventory.Bag]` return annotation (class-def time)
  in RawItemCache. Fixed the two annotations (56, 150) to `dict`. IMPORT-only fix.

- 2026-07-04: `ImportError: cannot import name 'HeroPartyMember' from 'PyParty'` at
  SharedMemory.py:16 - legacy PyParty types (annotation-only). Repointed the import
  in 3 files (GlobalCache/SharedMemory.py, shared_memory_src/AccountStruct.py,
  AllAccounts.py) to the reforged structs: `HeroPartyMember` from
  native_src.context.PartyContext; `PetInfoStruct as PetInfo` from
  native_src.context.WorldContext.

- 2026-07-04: `AttributeError: module 'PyCamera' has no attribute 'PyCamera'` -
  CameraCache.__init__ (import via GLOBAL_CACHE). Camera is NATIVE-BLOCKED (no
  CameraContext) - deferred _camera_instance to None; full Camera migration pending
  native work. (Camera.py + CameraCache.py; Camera reads have no reforged source.)
- 2026-07-04: `AttributeError: module 'PyMerchant' has no attribute 'PyMerchant'` -
  MerchantCache.__init__ (import via GLOBAL_CACHE). Migrated the merchant facade
  LIBRARY-WIDE (native getters already built): Merchant.py (facade -> inline
  transact_items/request_quote), MerchantCache.py (cache -> module-level helper
  callables _trader_buy/_merchant_sell/... for the action queue), items.py
  behaviortree (reached into the cache private). Sources/Nikon_Scripts (3) + Legacy
  DEMO/.lua are user-scripts, out of library scope.

- 2026-07-04: `AttributeError: module 'PyParty' has no attribute 'PyParty'` -
  PartyCache.__init__ (import via GLOBAL_CACHE). BIG data+action facade unit, migrated
  FULLY LIBRARY-WIDE (owner chose "full Party migration now"). Reforged data flow:
  party id + roster lists (players/heroes/henchmen/others) come from GWContext.Party
  (PartyInfoStruct); counts/flags/tick/actions come from PyParty free functions; hero
  flag-state, hero names, the player login roster (login_number == PlayerStruct
  .player_number) and pet info come from GWContext.World. Files:
  * Party.py - full rewrite; every party_instance() body remapped; helper
    Party._player_party(). hero.hero_id is now a plain int (was Hero obj) ->
    .hero_id.GetID()/.GetName() dropped; hero names via world hero_info.
  * PartyCache.py - full rewrite; reads delegate to the Party facade, mutating actions
    queue the PyParty free functions directly (same arg shape - no marshaling wrappers).
  * AgentPartyStruct.py - GetLoginNumber/GetPartyNumber delegate to Party.Players.
  * AccountStruct.py + AllAccounts.py - swept hero_data.hero_id.GetID()/GetName()
    (hero_data is HeroPartyMember) -> .hero_id / Party.Heroes.GetNameByAgentID(agent_id).
  NATIVE-BLOCKED remainder (marked, no-op/best-effort, runtime-only): UseHeroSkill
  (no binding, was already non-functional in legacy); the static hero name->id table
  (legacy PyParty.Hero) for heroes NOT in party (in-party heroes resolve via hero_info).
  Python-only, no rebuild (all PyParty party free functions were in the last build).

- 2026-07-04: `AttributeError: module 'PyParty' has no attribute 'PyParty'` handled
  (full Party migration, see above). Then the surface reached quest.
- 2026-07-04: `AttributeError: module 'PyQuest' has no attribute 'PyQuest'` - resolved
  by the quest NATIVE-BINDING PILOT (see docs/native-binding-coverage-audit.md): ported
  the ~20 missing legacy PyQuest functions into quest_bindings.cpp + rewrote Quest.py/
  QuestCache.py to the free functions (data via ctypes). Python fix clears the surface
  error even pre-build. AWAITING build+inject verify.
- 2026-07-04: `AttributeError: module 'PySkillbar' has no attribute 'Skillbar'` -
  SkillbarCache.__init__ (import via GLOBAL_CACHE). Fixed FULLY LIBRARY-WIDE (per owner
  cadence: fix each issue library-wide, not defer). Skillbar data via ctypes
  (GWContext.World.party_skillbars, selected by agent id) + PySkillbar free-function ops.
  * skillbar_bindings.cpp: added get_hovered_skill_id (binds existing GetHoveredSkill)
    and hero_use_skill (ported legacy: game_thread::Enqueue + agent target swap +
    ui::Keypress(ControlAction_HeroNSkill1 + skill_idx); hero bases are not uniformly
    strided so uses named enum constants).
  * WorldContext.py SkillbarStruct: FIXED a real layout bug - +0xA8 was wrongly modelled
    as a GW_Array cast_array; native GW::Context::Skillbar (skill.h) is h00A8[2] +
    casting@+0xB0 + h00B4[2]. Added `casting`, dropped the unused/garbage casted_skills.
  * Skillbar.py + SkillbarCache.py: rewritten to ctypes data + free-function ops.
  NEEDS BUILD (skillbar_bindings.cpp changed; no new .cpp so no CMake reconfigure). This
  build also covers the quest pilot.

- 2026-07-04: `ImportError: cannot import name 'BTAgents' from ...behaviourtrees_src`
  (BehaviourTrees.py:64, during __init__:84). MISLEADING message - Python's import_from
  rewrites an AttributeError raised while resolving the lazy attr into this generic text,
  hiding the real cause. Real cause (captured via a temp diagnostic in behaviourtrees_src/
  __init__.py __getattr__ that writes traceback to _btagents_error.txt): movement.py:73
  `def _log(..., message_type=Console.MessageType.Info)` - a DEFAULT ARG evaluated at
  import, where `Console = Py4GW.Console` (Console.py:25) and Py4GW.Console has no
  MessageType. Fix: repoint the alias Console.py:25 `Console = Py4GW.Console` ->
  `Console = PySystem.Console` (the authoritative console; MessageType/Log/... live on
  the PySystem.Console submodule). One line, library-wide (Console defined once).
  DIAGNOSTIC TECHNIQUE (reusable): when a surface ImportError says "cannot import name X"
  but X exists, the real error is an AttributeError masked by import_from - wrap the
  failing lazy import / from-import in try/except that dumps traceback.format_exc() to a
  file, re-inject, read it. A stubbed-native import repro does NOT reproduce these
  (native runtime-value divergence) - go straight to the in-game diagnostic.
  NOTE (separate, deferred - runtime, not import blockers): PySystem.Console.GetCredits()
  (ImGuisrc.py:3665) should be PySystem.get_credits(); Console.Console.MessageType
  double-Console typo (HotkeyManager.py:87,90) should be Console.MessageType.

### REQUIRED before callbacks go back on (runtime landmine, not a surface/import issue)
RawItemCache (ItemCache.py) still BUILDS `PyInventory.Bag(bag, str(bag))` at runtime
(line 78, in update()) and its whole consumer chain reads bag OBJECTS. Native
PyInventory.Bag is gone -> full dict migration needed: RawItemCache.update ->
`get_bag(bag)` dict; get_items/get_all_items/get_bag/get_bags -> dict access; and the
consumers: ~19 loops in InventoryCache.py (the get_bags() `bag.GetItems()`/`.GetSize()`/
`.GetItemCount()` loops previously left in place) + ItemCache.py consumers (lines
237-241, 655-656, 665-666). CAUTION: ItemCache.py mixes bag-dict items with
transitory `PyItem.PyItem` objects (lines 85-92 agent_id/agent_item_id, 104-105,
225/231/294/306/384) - those are NOT dicts; migrate block-by-block, do NOT token-sweep
`item.X`. This is the proper completion of the PyInventory unit (was under-scoped).

## Codebase-wide dead-API audit (RULE: fix every pattern EVERYWHERE, not one file)

Owner rule (2026-07-04): a dead-API pattern in one script is in several more - every
fix must be a codebase-wide sweep of the pattern, retroactive across the WHOLE
project (Py4GWCoreLib + Widgets + root scripts), not just the file that errored.

Audit (whole project, occurrences). Some are already partly done in Py4GWCoreLib
core only; the counts show how much is still spread across the codebase:

- `PyPointers`            19 files / 43x  (removed from 13 context files; ~6 more remain)
- `PyInventory.Bag`       19 files / 42x  (done in ~5 core files; RawItemCache chain + rest remain)
- `PySkillbar.Skillbar`   10 files / 37x  (native-blocked unit - GetPlayerSkillbar etc. unbound)
- `PyInventory.PyInventory` 16 files / 27x
- `PyItem.PyItem`          6 files / 18x  (PyItem.PyItem EXISTS now - these are FINE, verify)
- `Py2DRenderer`           9 files / 11x  (-> PyDXOverlay; done in 2 core files, 7 remain)
- `PyParty.PyParty`       10 files / 11x  (API-shape unit)
- `PyEffects.PyEffects`    1 file  / 8x   (1 non-core file still has it)
- `PyMerchant.PyMerchant`  7 files / 7x   (native getters done; py facades remain)
- `PyAgent.PyAgent`        5 files / 6x   (+ from PyAgent import Profession/AttributeClass)
- `PyCombatEvents`         4 files / 5x   (-> PyAgentEvents; done in 2, 2 remain)
- `PyPlayer.PyPlayer`      3 files / 5x
- `PyQuest.PyQuest`        4 files / 4x   (native-blocked unit)
- `PyCamera.PyCamera`      2 files / 2x   (native-blocked: no CameraContext; DEFERRED for surface)

Also import-time dead `from Py* import <name>`: `from PyItem import DyeColor`
(bound as DyeColorClass - name gap), `from PySkillbar import SkillbarSkill`,
`from PyAgent import PyAgent/Profession/AttributeClass`, `from PyParty import Hero`.

The surface is blocked mainly by GLOBAL_CACHE cache classes that instantiate a dead
facade in `__init__` (runs at import via GLOBAL_CACHE): CameraCache (done - deferred),
and likely MerchantCache/QuestCache/PartyCache/SkillCache/etc. next.

### Comprehensive plan (chunked)
1. Surface (callbacks off): sweep each import-time dead ref codebase-wide until
   `import Py4GWCoreLib` completes. GLOBAL_CACHE caches: migrate or defer the dead
   `PyX.PyX()` in __init__. Mechanical spread patterns to sweep whole-project:
   PyPointers (remove dead imports), Py2DRenderer->PyDXOverlay, PyCombatEvents->
   PyAgentEvents, dead `from Py* import`. Native-blocked (Camera/Quest/Skillbar):
   defer the import-time trigger, mark blocked.
2. Then the API-shape facade units (Party/Merchant/Player/Inventory.Bag/ItemCache
   RawItemCache) migrated comprehensively (facade + cache + ALL callers), block by
   block (bag-dict vs PyItem object care).
3. Native-blocked units (Camera CameraContext, Quest, Skillbar) need C++ work +
   rebuild - do after the pure-Python surface is clean.

## Chunking

Each chunk = enable a subset of callbacks (or fix one surface error), rebuild, inject,
observe. Record findings back here and in `python-migration-progress.md` every step.

## Open questions to resolve next session

- Does the surface come up clean with callbacks off? (Step 1.) If NOT, the crash is
  not purely callback-driven and the isolation assumption needs revisiting.
- Is the bad callable one specific callback, or does corruption from an earlier
  Python path (context ctypes access?) taint an arbitrary later callback? The latter
  matches the Vec2-era pattern and is the harder case.

## Hard rules (unchanged)

- Never modify the callback DISPATCH logic (the `g_process_callbacks` gate is the
  only allowed addition, owner-requested for debugging).
- Trust the owner's empirical diagnosis; bisect around the change they name.
- No shims; no auto-builds (owner rebuilds).
