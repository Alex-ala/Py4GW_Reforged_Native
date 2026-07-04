# Shared Memory Handover — new C++ contract (for the Python adaptation)

Status: 2026-07-03. The C++ publisher was reworked into a per-module
subscription model. **This changes the shared-memory wire contract end to end.**
The Python reader (`Py4GWCoreLib/native_src/ShMem/SysShaMem.py`) currently
assumes the OLD layout and will not read the new region correctly until adapted.
Nothing on the Python side was touched yet — this document is the spec to adapt
against.

**Target is 32-bit (Win32).** Pointers (`uintptr_t`, `void*`) are **4 bytes**.
Use `ctypes.c_uint32` / `c_void_p` (4 bytes here) accordingly.

---

## 0. TL;DR of what changed

| | OLD contract (what SysShaMem.py reads today) | NEW contract |
|---|---|---|
| Layout | fixed: `[Header][AgentArray][Pointers]`, offsets computed from struct sizes | `[Header][Descriptor table][Payloads...]`, **discover segments by name via the descriptor table** |
| Header | 5 fields (version,total_size,sequence,process_id,window_handle) | 9 fields (adds frame_counter, last_publish_tick, descriptor_count, descriptor_offset) — see §2 |
| Segment count | 2 | 22 (and growing/shrinking as subscriptions are toggled) |
| Segment identity | positional | **by name** (`char[64]` in each descriptor) |
| Pointers payload | `Pointers_SHMemStruct` (15 `c_void_p`) | `RuntimePointersSnapshot` (19 `uintptr_t`) — field set changed, see §5 |
| Agent payload | `AgentArraySHMemStruct` | `AgentArraySnapshot` (same intent; verify field-for-field, see §5) |
| New payloads | — | full context structs (`runtime.ctx.*`), `runtime.map.summary` |
| Disable semantics | n/a | disabled segment publishes **all zeros** in place (layout unchanged); commenting a subscription removes it (layout shifts) |

**The single biggest change:** stop computing offsets from hardcoded struct
sizes. Read `descriptor_offset` + `descriptor_count` from the header, parse the
descriptor table, and look up each segment **by name** to get its `offset`/`size`.
That makes the reader immune to added/removed/toggled segments.

---

## 1. Transport (unchanged mechanism)

- One Windows named file mapping (`CreateFileMappingW(INVALID_HANDLE_VALUE, PAGE_READWRITE, ...)`).
- **Name**: `Py4GW_Runtime_<pid>_<hwnd>` (format `"%ls_%lu_%p"`, prefix `Py4GW_Runtime`). Get it from Python via `Py4GW.SharedMemory.get_name()` (already exposed) — do not hardcode.
- Python opens it exactly as today: `multiprocessing.shared_memory.SharedMemory(name=..., create=False)`.
- Publisher lifecycle: created at `Py4GW_Initialize`, `manager.Update()` runs first thing every frame (`Py4GW.cpp` update loop), destroyed at shutdown.
- Metadata helpers already bound under `Py4GW.SharedMemory`: `is_ready()`, `get_name()`, `get_size()`, `get_sequence()`, `get_frame_counter()`, `list_segments()`, `get_segment(name)` (returns dict with offset/size/interval/publish_count/last_result), `set_update_interval(name,frames)`, `publish_now(name)`.

---

## 2. Header layout — `SharedMemoryHeader` (48 bytes, natural alignment, NOT packed)

| field | type | offset | notes |
|-------|------|--------|-------|
| version | uint32 | 0 | currently **2** (`kSharedMemoryVersion`) |
| total_size | uint32 | 4 | whole region size |
| sequence | uint32 | 8 | **seqlock**: odd = write in progress |
| process_id | uint32 | 12 | publisher PID |
| window_handle | uint64 | 16 | (kept 0 currently) |
| frame_counter | uint64 | 24 | increments every `Update()` — use to detect fresh frames |
| last_publish_tick | uint64 | 32 | `GetTickCount64()` of last publish |
| descriptor_count | uint32 | 40 | number of segments |
| descriptor_offset | uint32 | 44 | byte offset of the descriptor table (== 48) |

Python ctypes: plain `Structure` (no `_pack_`, natural align matches). The first
five fields match the old header, so extend the existing header struct rather
than replace it.

## 3. Descriptor table — `SegmentDescriptor` (96 bytes each, NOT packed)

Located at `header.descriptor_offset`, `header.descriptor_count` entries.

| field | type | offset | notes |
|-------|------|--------|-------|
| name | char[64] | 0 | ASCII, NUL-terminated (`kSharedMemorySegmentNameCapacity` = 64) |
| offset | uint32 | 64 | payload byte offset from region base |
| size | uint32 | 68 | payload size |
| update_interval_frames | uint32 | 72 | 1 = every frame |
| publish_count | uint32 | 76 | diagnostics |
| last_published_frame | uint64 | 80 | |
| last_result | uint32 | 88 | 0=Ok,1=NotDue,2=CallbackFailed,3=Exception |
| reserved | uint32 | 92 | |

## 4. Reader algorithm (recommended)

```
open mapping by name (Py4GW.SharedMemory.get_name())
loop up to 3x:                       # seqlock
    h = read header
    if h.sequence & 1: retry         # writer active
    seq0 = h.sequence
    descriptors = parse h.descriptor_count entries at h.descriptor_offset (96 bytes each)
    seg = { d.name: (d.offset, d.size) for d in descriptors }
    copy the payload bytes you need at seg[name]
    if read header.sequence != seq0: retry
    else break
parse each copied payload with the matching ctypes struct (below)
```

Key differences from today's reader: it must (a) use the 48-byte header, (b)
build the name->offset map from the descriptor table instead of the fixed
`[agent][pointers]` offsets, (c) treat an all-zero payload as "not available
this frame" (see §7).

---

## 5. Segment catalog (current layout order)

Order below is the current layout order (aggregator order); **do not rely on
order — look up by name**. Mode: STRUCT = raw game context bytes; SNAPSHOT =
derived POD; (POINTER = 8/4-byte address — none yet, but `SubscribePointer`
produces one and Python would `cast` it).

| # | name | payload type | mode | map-ready gated | ctypes mirror |
|---|------|--------------|------|-----------------|---------------|
| 1 | runtime.pointers | RuntimePointersSnapshot | SNAPSHOT | yes | §5.1 (replaces old Pointers_SHMemStruct) |
| 2 | runtime.ctx.char | CharContext | STRUCT | yes | native_src/context CharContextStruct |
| 3 | runtime.ctx.world | WorldContext | STRUCT | yes | WorldContext |
| 4 | runtime.ctx.game | GameContext | STRUCT | yes | GameContext |
| 5 | runtime.ctx.gameplay | GameplayContext | STRUCT | yes | GameplayContext |
| 6 | runtime.ctx.account | AccountContext | STRUCT | yes | (add mirror) |
| 7 | runtime.ctx.pregame | PreGameContext | STRUCT | yes | PreGameContext |
| 8 | runtime.ctx.text_parser | TextParser | STRUCT | yes | TextContext |
| 9 | runtime.ctx.salvage | SalvageSessionInfo | STRUCT | yes | (add mirror) |
| 10 | runtime.ctx.agent | AgentContext | STRUCT | yes | AgentContext/AgentArrayStruct |
| 11 | runtime.agents | AgentArraySnapshot | SNAPSHOT | yes | §5.2 (replaces old AgentArraySHMemStruct) |
| 12 | runtime.ctx.map | MapContext | STRUCT | yes | MapContext |
| 13 | runtime.ctx.mission_map | MissionMapContext | STRUCT | yes | MissionMapContext |
| 14 | runtime.ctx.world_map | WorldMapContext | STRUCT | yes | WorldMapContext |
| 15 | runtime.map.summary | MapContextSnapshot | SNAPSHOT | (map ctx only) | §5.3 |
| 16 | runtime.ctx.party | PartyContext | STRUCT | yes | PartyContext |
| 17 | runtime.ctx.item | ItemContext | STRUCT | yes | (add mirror) |
| 18 | runtime.ctx.guild | GuildContext | STRUCT | yes | GuildContext |
| 19 | runtime.ctx.trade | TradeContext | STRUCT | yes | (add mirror) |
| 20 | runtime.ctx.friends | FriendList | STRUCT | yes | (add mirror) |
| 21 | runtime.ctx.camera | Camera | STRUCT | yes | (add mirror) |
| 22 | runtime.ctx.render | GwDxContext | STRUCT | yes | (add mirror) |

Every `runtime.ctx.*` payload is the raw bytes of that C++ context struct (the
same struct the ctypes mirrors in `native_src/context` already describe with
`_pack_=1` + offset asserts). **The struct's internal pointers are live
in-process addresses** — same-process ctypes can follow them (as today); a
cross-process reader cannot.

### 5.0 Dynamic-list pointer segments (`runtime.ptr.*`)

Each is a single `uintptr_t` (4 bytes on Win32) = the address of a dynamic
game list/array. Read the value and `cast(ptr, POINTER(<ctypes struct>))` in
Python, exactly like the agent array works today. These carry NO element data
themselves — they are pointers, materialized per-element on the Python side.
Zero means "not available this frame".

| segment | points at | element ctypes |
|---------|-----------|----------------|
| runtime.ptr.agent_array | AgentArray | AgentStruct (per agent) |
| runtime.ptr.map_agent_array | MapAgentArray | MapAgent |
| runtime.ptr.npc_array | NPCArray | NPC |
| runtime.ptr.player_array | PlayerArray | PlayerPartyMember/Player |
| runtime.ptr.party_effects | AgentEffectsArray | AgentEffects |
| runtime.ptr.item_array | ItemArray | Item |
| runtime.ptr.inventory | Inventory | Inventory/Bag |
| runtime.ptr.item_formulas | ItemFormula[] | ItemFormula |
| runtime.ptr.merchant_items | MerchItemArray | uint32 item ids |
| runtime.ptr.skill_array | Skill[] (global skill data) | Skill |
| runtime.ptr.attribute_info | AttributeInfo[] | AttributeInfo |
| runtime.ptr.guild_array | GuildArray | Guild |
| runtime.ptr.quest_log | QuestLog | Quest |
| runtime.ptr.area_info_array | AreaInfo[] | AreaInfo |
| runtime.ptr.map_type_instance_infos | MapTypeInstanceInfo[] | MapTypeInstanceInfo |
| runtime.ptr.pathing_map | PathingMapArray | PathingMap |
| runtime.ptr.instance_info | InstanceInfo | InstanceInfo |
| runtime.ptr.region_id | ServerRegion | (enum uint32) |

**Deferred until the C++ getter is implemented** (declared-only today, not yet
published): `GetPlayerBuffs`, `GetPlayerEffects`, `GetPlayerEffectsArray`,
`GetMissionMapIconArray`, `GetSettings`, `GetActiveQuest`, `GetActiveTitle`,
`GetChatLog`. They will appear as further `runtime.ptr.*` segments when wired.

### 5.1 `RuntimePointersSnapshot` (segment `runtime.pointers`) — 19 x uint32 (Win32)

Order (offsets step by 4 bytes): `mission_map_context, world_map_context,
gameplay_context, instance_info, map_context, game_context, pregame_context,
world_context, char_context, agent_context, guild_context, party_context,
trade_context, item_context, friend_list, render_context, text_parser, camera,
window_handle_ptr`.

This is the renamed/extended successor to the old `Pointers_SHMemStruct` (which
had 15 `c_void_p`). Fields added/renamed: now includes `instance_info`,
`gameplay_context`, `render_context`, `text_parser`, `camera`,
`window_handle_ptr`; there is no separate `CinematicContext`/`AvailableCharacters`/
`ServerRegionContext`/`CharContext`-vs order match — **re-mirror this exactly**,
it is not field-compatible with the old struct.

### 5.2 `AgentArraySnapshot` (segment `runtime.agents`) — `#pragma pack(1)`

```
uint32 max_size            # == 300
uint32 count               # valid entries
AgentSnapshotEntry entries[300]        # { uintptr ptr(4); uint32 agent_id; }  (8 bytes each on Win32)
AgentRefSnapshotArray all              # { uint32 count; RefEntry entries[300]{uint32 agent_id; uint32 index;} }
AgentRefSnapshotArray ally
AgentRefSnapshotArray neutral
AgentRefSnapshotArray enemy
AgentRefSnapshotArray spirit_pet
AgentRefSnapshotArray minion
AgentRefSnapshotArray npc_minipet
AgentRefSnapshotArray living
AgentRefSnapshotArray item
AgentRefSnapshotArray owned_item
AgentRefSnapshotArray gadget
AgentRefSnapshotArray dead_ally
AgentRefSnapshotArray dead_enemy
```
Cross-check against the current `AgentArraySSM.py` wrapper — the category set
matches, but confirm entry sizes/order and the pack. `entries[i].ptr` is the
live agent pointer; `cast(ptr, POINTER(AgentStruct))` as today.

### 5.3 `MapContextSnapshot` (segment `runtime.map.summary`) — `#pragma pack(1)`

```
float    map_boundaries[5]
float    trapezoid_bounds[6]
uintptr  terrain, zones, props, pathing_sub1, pathing_sub2
uint32   pathing_map_count, pathing_block_count, total_trapezoid_count,
         prop_model_count, prop_array_count
uintptr  instance_info
uint32   instance_type
uintptr  current_map_info
uint32   terrain_count
```
New payload (no old equivalent) — add a fresh ctypes mirror.

---

## 6. Toggle / disable semantics (how a struct turns off)

Each subscription line in `src/GW/<module>/<module>_shared_memory.cpp` has a
trailing `enabled` flag, and there is a runtime `Manager::SetSegmentEnabled(name, bool)`.

- **enabled == true**: writer copies the data each frame.
- **enabled == false**: the Manager writes **all zeros** into the same-size
  slot. The descriptor still exists, `offset`/`size` unchanged — **layout does
  not shift**. A reader keying by name keeps working; the payload is just zeros.
- **commenting out the subscribe line**: removes the segment entirely →
  `descriptor_count` drops and every later segment's `offset` shifts. This is
  fine *because* the reader keys by name (never by fixed offset). This is the
  intended way to permanently drop a struct.

Reader rule: **look up by name every read; never cache offsets across frames**
(a runtime toggle keeps offset stable, but a rebuild/relaunch with commented
lines will not).

## 7. Gotchas the Python side must handle

1. **Map-ready gating.** `runtime.ctx.*` (and the snapshots) publish only when a
   map is loaded and not observing/loading. During load screens their slots are
   **zeros**. Detect and treat all-zero as "stale/unavailable this frame" rather
   than parsing garbage.
2. **In-process pointers only.** STRUCT payloads and the pointer snapshot carry
   live game addresses. Valid only inside the game process (same as today's
   ctypes path). A separate process must use the value fields, not the pointers.
3. **Seqlock.** Honor the odd-sequence / re-read protocol (§4). The publisher
   bumps `sequence` at the start and end of each frame's write batch.
4. **32-bit pointers.** `uintptr_t` = 4 bytes. Any struct with pointer fields
   must mirror them as 4-byte on this build.
5. **Version.** Header `version` is 2. Gate the reader on it; bump on any header
   change.
6. **Names are the API.** `list_segments()` / `get_segment(name)` are the source
   of truth for what exists and where. Prefer them (or the descriptor table) over
   any hardcoded assumption.

## 8. Python migration checklist (when appropriate)

- [ ] Replace the header ctypes struct with the 48-byte `SharedMemoryHeader` (§2).
- [ ] Add a `SegmentDescriptor` ctypes struct (96 bytes, §3) and a descriptor-table parser.
- [ ] Rewrite `SysShaMem.py` to discover segments by name from the descriptor table (drop the fixed `[agent][pointers]` offset math).
- [ ] Re-mirror `runtime.pointers` as `RuntimePointersSnapshot` (§5.1) — field set changed.
- [ ] Verify/adjust the `runtime.agents` mirror vs `AgentArraySnapshot` (§5.2).
- [ ] Add mirrors for `runtime.map.summary` (§5.3) and any `runtime.ctx.*` structs you want to read directly (most already exist in `native_src/context`).
- [ ] Repoint the context facades (`CharContext`, `MapContext`, ... `_update_ptr`) to source their pointer from `runtime.pointers` (unchanged idea, new field layout) — or read the context bytes directly from `runtime.ctx.*`.
- [ ] Add the all-zero / map-ready guard on every parse.
- [ ] Keep the PreUpdate refresh ordering (reader before the context `_update_ptr` callbacks).

## 9. Where the C++ lives (for cross-reference)

- Manager + layout: `include/GW/shared_memory/manager.h`, `src/GW/shared_memory/manager.cpp`
- Registrars: `include/GW/shared_memory/segments.h`
- Subscriptions (one file per module): `src/GW/<module>/<module>_shared_memory.cpp`
- Snapshot PODs: `RuntimePointersSnapshot` in `include/GW/context/context.h`; `AgentArraySnapshot` in `include/GW/context/agent.h`; `MapContextSnapshot` in `include/GW/context/map.h`
- Python bindings for metadata: `src/base/python_runtime.cpp` (`Py4GW.SharedMemory`)
- Categorization reference: `docs/shared-memory-context-catalog.csv`
