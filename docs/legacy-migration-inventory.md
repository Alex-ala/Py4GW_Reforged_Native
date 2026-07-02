# Legacy Migration Inventory and Checklist

Status: DRAFT - inventory in progress (survey agents running).
Generated: 2026-07-02.
Legacy source: `C:\Users\Apo\Py4GW` (own layer: `src/`, `include/`; vendored library: `vendor/gwca/`).
Target: `C:\Users\Apo\Py4GW_Reforged`.

This document is the authoritative per-file inventory of the legacy project,
classified by what each piece should become in the new structure, and marked
with its current migration status. It is the base checklist for all future
migration planning.

## Classification taxonomy

Every legacy file/component is classified as exactly one of:

- **MODULE** - subsystem with its own lifecycle (`Initialize`/`Shutdown`,
  hooks, resolved symbols). Target: `include/GW/<module>/` +
  `src/GW/<module>/` (GW-facing) or a top-level module dir (project-facing).
- **SUBMODULE / COMPONENT** - a distinct component that belongs inside a
  parent module rather than standing alone (e.g. a catalog, a bridge, an
  addon). Listed under its parent module.
- **SHARED INFRA (base/)** - project infrastructure with no GW specificity,
  used by several modules and coordinated from the top-level lifecycle.
  Target: `include/base/` + `src/base/`.
- **COMMON (GW/common/)** - shared GW protocol/type surface with no
  lifecycle: packets, opcodes, containers, constants tables.
- **CONTEXT / STRUCT (GW/context/)** - shared GW entity/context memory
  layouts and helper methods on GW types, no lifecycle.
- **CONSTANT** - pure data tables (IDs, names, catalogs). Target:
  `GW/common/constants/` for GW data; module-owned header for module data.
- **HELPER** - stateless utility code. Target: `base/` if project-wide,
  module header if module-local.
- **BINDING** - pybind11 Python surface. Target: `<module>_bindings.cpp`
  inside the owning module. Bindings are never migrated automatically with
  the native manager (migration guide scope rule).
- **OBSOLETE** - superseded by the new architecture; intentionally not
  migrated.

Migration status values: `MIGRATED`, `PARTIAL`, `NOT MIGRATED`, `OBSOLETE`.

---

# Part 1 - Legacy GWCA vendor library (`vendor/gwca/`)

Symbol-level parity for this layer is tracked in `parity-report-2026-06-28.md`
and `pattern_parity_audit.md`. This section is the file-level inventory and
classification. Verified against the working tree on 2026-07-02.

> NOTE: the 2026-06-28 parity report's line references into `include/GW/ui/ui.h`
> are stale. The three critical UIMgr bugs it lists ARE fixed, but the symbols
> were relocated during a later refactor: `GetTitleFn` (now `__fastcall`) and
> `SetTooltipFn` live in `src/GW/ui/ui.cpp`; `ChangeTargetUIMsg` / `FloatingWindow`
> live in `include/GW/context/ui.h`; `TooltipType` lives in
> `include/GW/common/constants/ui.h`.

## 1.1 Managers (`Include/GWCA/Managers/` + `Source/*Mgr.cpp`) - all MODULE

Canonical target shape per module: `include/GW/<m>/<m>.h`,
`src/GW/<m>/{<m>.cpp, <m>_methods.cpp, <m>_patterns.cpp, <m>_bindings.cpp}`,
`offsets/<m>.json`. A dash means the piece intentionally does not exist or is
a gap (see notes).

| Legacy | Class | New module | h | cpp | methods | patterns | bindings | offsets json | kInitSteps | Status | Notes |
|---|---|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|---|---|
| AgentMgr | MODULE | `GW/agent` | x | x | x | x | x | agent.json | #12 | MIGRATED | `GetMouseoverId` was always a stub in legacy |
| CameraMgr | MODULE | `GW/camera` | x | x | x | x | x | camera.json | #5 | MIGRATED | |
| ChatMgr | MODULE | `GW/chat` | x | x | x | x | x | chat.json | #16 | MIGRATED | |
| EffectMgr | MODULE | `GW/effects` | x | x | x | x | x | effects.json | #8 | MIGRATED | |
| EventMgr | MODULE | `GW/events` | x | x | - | x | - | events.json | #9 | MIGRATED (native) | no `_methods` (lifecycle-shaped); **no bindings** |
| FriendListMgr | MODULE | `GW/friend_list` | x | x | x | x | x | friend_list.json | #10 | MIGRATED | |
| GameThreadMgr | MODULE | `GW/game_thread` | x | x | x | x | x | game_thread.json | #1 | MIGRATED | |
| GuildMgr | MODULE | `GW/guild` | x | x | x | - | x | guild.json | #15 | MIGRATED | pure context-based, no `_patterns` needed |
| ItemMgr | MODULE | `GW/item` | x | x | x | x | x | item.json | #17 | MIGRATED | |
| MapMgr | MODULE | `GW/map` | x | x | x | x | x | map.json | #14 | MIGRATED | EnterChallengeMission hook restored post-report |
| MemoryMgr | SHARED INFRA | `base/memory_manager` | x | x | - | - | - | memory.json | #6 (scan step) | MIGRATED | deliberately NOT a GW module (multi-consumer); `include/GW/memory/` dir is empty/vestigial |
| MerchantMgr | MODULE | `GW/merchant` | x | x | x | x | x | merchant.json | #19 | MIGRATED | merchant.json gap from report now closed |
| PartyMgr | MODULE | `GW/party` | x | x | x | x | x | party.json | #21 | MIGRATED | |
| PlayerMgr | MODULE | `GW/player` | x | x | x | x | x | player.json | #11 | MIGRATED | |
| QuestMgr | MODULE | `GW/quest` | x | x | x | x | x | quest.json | #13 | MIGRATED | |
| RenderMgr | MODULE | `GW/render` | x | x | x | x | x | render.json | #3 | MIGRATED | ScreenCapture hook restored post-report |
| SkillbarMgr | MODULE | `GW/skillbar` | x | x | x | x | x | skillbar.json | #20 | MIGRATED | |
| StoCMgr | MODULE | `GW/stoc` | x | x | x | x | - | stoc.json | #2 | MIGRATED (native) | **no bindings** |
| TradeMgr | MODULE | `GW/trade` | x | x | x | - | x | (none) | #18 | MIGRATED | no scans in legacy either - no json is correct |
| UIMgr | MODULE | `GW/ui` | x | x | x | x | - | ui.json | #4 | MIGRATED (native) | **no bindings**; largest remaining binding surface |

Framework glue (not modules):

| Legacy | Class | New form | Status |
|---|---|---|---|
| `Managers/Module.h` | OBSOLETE (framework) | replaced by `struct InitStep` + `kInitSteps` table in `src/GW/GuildWars.cpp` | MIGRATED (re-architected) |
| `GWCA.h` / `Source/GWCA.cpp` | MODULE (orchestrator) | split: init orchestration -> `src/GW/GuildWars.cpp`; base context resolution -> `GW/context` module (`src/GW/context/context.cpp`) | MIGRATED |

kInitSteps order (21 steps): game_thread, stoc, render, ui, camera,
memory_manager (scan), context, effects, events, friend_list, player, agent,
quest, map, guild, chat, item, trade, merchant, skillbar, party; then
`MemoryPatcher::EnableHooks()`.

## 1.2 GameEntities/ + Context/ -> `GW/context/` - all CONTEXT / STRUCT

All 15 legacy `Context/` headers and all 16 `GameEntities/` headers are
migrated into `include/GW/context/`. Five new files merge a `*Context.h` with
its entity header: `agent.h`, `guild.h`, `item.h`, `map.h`, `party.h`.

| Legacy Context/ | New | Legacy GameEntities/ | New |
|---|---|---|---|
| AccountContext.h | account.h | Agent.h | agent.h (merged) |
| AgentContext.h | agent.h (merged) | Attribute.h | attribute.h |
| CharContext.h | character.h | Camera.h | camera.h |
| Cinematic.h | cinematic.h | Friendslist.h | friend_list.h |
| GadgetContext.h | gadget.h | Guild.h | guild.h (merged) |
| GameContext.h | game.h | Hero.h | hero.h |
| GameplayContext.h | gameplay.h | Item.h | item.h (merged) |
| GuildContext.h | guild.h (merged) | Map.h | map.h (merged) |
| ItemContext.h | item.h (merged) | Match.h | match.h |
| MapContext.h | map.h (merged) | NPC.h | npc.h |
| PartyContext.h | party.h (merged) | Party.h | party.h (merged) |
| PreGameContext.h | pregame.h | Pathing.h | pathing.h |
| TextParser.h | text_parser.h | Player.h | player.h |
| TradeContext.h | trade.h | Quest.h | quest.h |
| WorldContext.h | world.h | Skill.h | skill.h |
| | | Title.h | title.h |

New-side-only context files (no single legacy counterpart): `chat.h`,
`context.h` (aggregator, part of the `context` module), `render.h`, `ui.h`
(UI structs formerly inline in legacy `UIMgr.h`, incl. `ChangeTargetUIMsg`,
`FloatingWindow`).

Entity helper sources: legacy `Source/Skill.cpp` -> `src/GW/context/skill.cpp`;
`src/GW/context/item.cpp` also exists (item helpers). Legacy `GamePos.cpp` was
folded into header-only `GW/common/game_pos.h`.

Status: **MIGRATED, 100% coverage** (parity report: 110/110 types).

## 1.3 Constants/ -> `GW/common/constants/` - all CONSTANT

| Legacy | New | Status |
|---|---|---|
| AgentIDs.h | agent_ids.h | MIGRATED |
| Constants.h | constants.h + per-module splits | MIGRATED |
| ItemIDs.h | item_ids.h | MIGRATED |
| Maps.h | maps.h | MIGRATED |
| QuestIDs.h | quest_ids.h | MIGRATED |
| Skills.h | skills.h | MIGRATED |

New-side per-module constant headers split out of the legacy monolith:
`agent.h`, `chat.h`, `events.h`, `friend_list.h`, `hero.h`, `item.h`, `map.h`,
`render.h`, `ui.h` (holds `TooltipType`).

## 1.4 GameContainers / Packets / Utilities / Logger

| Legacy | Class | New | Status |
|---|---|---|---|
| GameContainers/Array.h | COMMON | `GW/common/gw_array.h` | MIGRATED |
| GameContainers/List.h | COMMON | `GW/common/gw_list.h` | MIGRATED |
| GameContainers/GamePos.h (+ GamePos.cpp) | COMMON | `GW/common/game_pos.h` (header-only) | MIGRATED |
| Packets/Opcodes.h | COMMON | `GW/common/opcodes.h` | MIGRATED |
| Packets/StoC.h | COMMON | `GW/common/stoc.h` | MIGRATED |
| Utilities/Scanner.h (+ .cpp) | SHARED INFRA | `base/scanner.{h,cpp}` | MIGRATED |
| Utilities/Hooker.h (+ .cpp) | SHARED INFRA | `base/hooker.{h,cpp}` | MIGRATED |
| Utilities/Hook.h | SHARED INFRA | `base/hook_types.h` | MIGRATED |
| Utilities/MemoryPatcher.h (+ .cpp) | SHARED INFRA | `base/memory_patcher.{h,cpp}` | MIGRATED |
| Utilities/FileScanner.h (+ .cpp) | SHARED INFRA | `base/file_scanner.{h,cpp}` | MIGRATED |
| Utilities/Debug.h (+ Debug.cpp) | SHARED INFRA | `base/panic.{h,cpp}` (+ `error_handling.h`) | MIGRATED |
| Utilities/Export.h | OBSOLETE | (none) - DLL export macros unneeded | OBSOLETE |
| Utilities/Macros.h | OBSOLETE | (none) - helper macros dropped/inlined | OBSOLETE |
| Logger/Logger.h | SHARED INFRA | `base/logger.h` | MIGRATED |
| Source/stdafx.{h,cpp} | OBSOLETE | (none) - PCH dropped | OBSOLETE |

New-side base/ infrastructure with no legacy GWCA counterpart:
`patterns.{h,cpp}` (JSON offset resolver engine), `CrashHandler.{h,cpp}`
(+ `offsets/crash.json`), `process_manager`, `python_runtime`, `timer.h`,
`bind_helpers.h`, `imvec_caster.h`.

## 1.5 GWCA-layer open gaps

- [ ] `GW/stoc` bindings (`stoc_bindings.cpp`) - no Python surface yet
- [ ] `GW/ui` bindings (`ui_bindings.cpp`) - largest unbound surface
- [ ] `GW/events` bindings - no Python surface yet
- [ ] `GW/context` bindings - no Python surface yet
- [ ] Vestigial empty dirs to remove or fill: `include/GW/memory/`,
      `include/GW/skills/` + `src/GW/skills/`, `include/GW/game_entities/` +
      `src/GW/game_entities/`
- [ ] Modules with sources but NOT wired into `kInitSteps`: `gw_dat_reader`,
      `ping`, `shared_memory` (see Part 2 for their legacy counterparts)
- [ ] UIMgr known deferrals (unchanged from parity report): command-line
      pref get/set, `AsyncDecodeStringPtr` hook wiring (`ui_hooks.cpp`),
      `Default_UICallback`, controller-mode stubs

---

# Part 2 - Legacy Py4GW own layer (`Py4GW/src`, `Py4GW/include`)

(inventory in progress - survey agent running)

# Part 3 - New-tree structure (target side)

## 3.1 Canonical module shape (observed conventions)

- Standard GW module = `include/GW/<m>/<m>.h` + `src/GW/<m>/{<m>.cpp,
  <m>_methods.cpp, <m>_patterns.cpp, <m>_bindings.cpp}` + `offsets/<m>.json`.
- `_patterns.cpp` + json exist only for modules that scan game memory;
  pure-logic modules (guild, trade) correctly omit them.
- `_bindings.cpp` exists only for Python-exposed modules; engine-internal
  modules (context, stoc, ui, events) currently omit bindings.
- `_methods.cpp` exists only when the module has a public callable surface
  beyond lifecycle.
- Native lifecycle is one `kInitSteps` table in `src/GW/GuildWars.cpp`
  (21 steps); the Python module import list in
  `src/base/python_runtime.cpp::Initialize()` is a separate, second wiring
  point - **both** must be updated when adding a bound module.

## 3.2 offsets/ mapping (22 files)

20 map 1:1 to GW modules (`agent, camera, chat, context, effects, events,
friend_list, game_thread, guild, gw_dat_reader, item, map, merchant, party,
player, quest, render, skillbar, stoc, ui`). Two map to base/ infrastructure:
`crash.json` -> `base/CrashHandler`, `memory.json` -> `base/memory_manager`.
GW modules with no json (by design or gap): `ping`, `shared_memory`, `trade`.

## 3.3 Live Python bindings (authoritative list)

Imported in `python_runtime::Initialize()` (25 modules): `Py4GW` (inline:
`Console`, `SharedMemory`, minimal `ImGui` submodules), `PySystem`,
`PySettings`, `PyProfiler`, `PyCallback`, `PyListeners`, `PyAgent`,
`PyCamera`, `PyChat`, `PyEffects`, `PyFriendList`, `PyGameThread`, `PyGuild`,
`PyItem`, `PyMap`, `PyMerchant`, `PyParty`, `PyPing`, `PyPlayer`, `PyQuest`,
`PyRender`, `PySkillbar`, `PyKeystroke`, `PyMouse`, `PyImGui`.

Not bound / not imported: `PyStoC`, `PyUI`, `PyTrade`, `PyContext`,
`PyEvents` (native modules exist; no Python surface yet).

## 3.4 Top-level (non-GW) modules

| Module | Class | Purpose | Python module |
|---|---|---|---|
| `base/` | SHARED INFRA | CrashHandler, hooker, logger, memory_manager, memory_patcher, patterns, process_manager, scanner, file_scanner, timer, panic, python_runtime, error_handling, bind_helpers, imvec_caster | `Py4GW` (inline in python_runtime.cpp) |
| `callback/` | MODULE | phased Python callback scheduler (PreUpdate/Data/Update), parity port of legacy PyCallback, timed via Profiler | `PyCallback` |
| `listeners/` | MODULE | named on/off listeners over native StoC events | `PyListeners` |
| `profiler/` | MODULE | per-metric rolling perf history (ports legacy MetricData) | `PyProfiler` |
| `settings/` | MODULE | INI-backed settings, Account vs Global scope (`docs/settings-ini-design.md`) | `PySettings` |
| `system/` | MODULE | console/message output singleton (`WriteConsoleMessage`) | `PySystem` |
| `virtual_input/` | MODULE | KeyHandler/MouseHandler posting input to the GW window (ports legacy VirtualInput) | `PyKeystroke`, `PyMouse` |
| `imgui/` | MODULE | ImGui manager, font manager, console UIs, addons; bindings assembled from per-domain registrars in `src/imgui/bindings/` | `PyImGui` (+ addon submodules) |

## 3.5 New-side modules with NO legacy counterpart

| Item | Purpose |
|---|---|
| `settings/` | new structured config layer (legacy had ad-hoc `Ini_handler.h`) |
| `system/` | central console/output system (legacy printed ad hoc) |
| `listeners/` | new event-subscription abstraction over StoC |
| `GW/shared_memory/` | shared-memory publisher (segments/header/sequence), bound as `Py4GW.SharedMemory` - re-architected vs legacy `SharedMemory.cpp` |
| `base/patterns` | JSON offset-resolver engine (replaces hardcoded legacy scans) |
| `base/CrashHandler` | crash sidecar with per-module attribution |
| `base/process_manager` | process services |

## 3.6 Structural anomalies / cleanup candidates

- Empty scaffold dirs: `include/GW/game_entities/` + `src/GW/game_entities/`,
  `include/GW/memory/`, `include/GW/skills/` + `src/GW/skills/`, top-level
  `include/common/` + `src/common/` - remove or fill.
- `GW/shared_memory` uses non-canonical file names (`manager.h/.cpp`).
- `gw_dat_reader` has no main `<module>.cpp` (only `arenanet_texture.cpp` +
  `_patterns.cpp`) and is not in `kInitSteps`.
- `ping`, `shared_memory`, `gw_dat_reader` are script-instantiated / lazy -
  intentional, but should be documented as such (this doc now does).

# Part 4 - Consolidated gap checklist

(pending Parts 2-3)
