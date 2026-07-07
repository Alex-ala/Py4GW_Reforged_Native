# Python Migration - Three-Layer Analysis (current state)

Date: 2026-07-04. Draft for review.

Scope: document the Python project (`C:\Users\Apo\Py4GW_Reforged\Py4GWCoreLib`)
as it exists now, across the three data layers it uses to reach game state, and
map each layer against the reforged native project
(`C:\Users\Apo\Py4GW_Reforged_Native`). This is the pre-migration survey; it
describes current reality and the adaptation each layer needs. It does not change
any code.

Reference legacy projects: `C:\Users\Apo\Py4GW` (native/C++ legacy),
`C:\Users\Apo\Py4GW_python_files` (python legacy).

## The core architectural shift

In legacy Py4GW the native DLL handed raw game-object pointers to Python through a
binding module (`PyPointers`, plus per-manager `Get*ContextPtr()` accessors). The
context classes cast those addresses onto ctypes structs.

In the reforged native project the C++ side **no longer shares pointers through
bindings**. Every base pointer is now published into a **shared memory** block
that Python attaches to and reads. Shared memory carries more than pointers - it
also carries copied-out struct snapshots (agent array, and a separate legacy
multibox block).

So the three Python-side data layers are:

1. **Bindings** - the `Py*` embedded modules (callable native surface).
2. **Shared memory** - the block the native side publishes; Python attaches read-only.
3. **Native contexts** - `native_src/context/*.py`, ctypes struct views laid over
   pointers that (now) come from shared memory.

The migration, in the owner's stated order: fix the **bindings** first, then fix
**where pointers are sourced** (pypointers -> shared memory) so the **contexts**
load, and it is almost ready.

---

## Layer 1 - Bindings

The Python library imports `Py*` modules and calls into them. The native rewrite
changed most of these to some extent. Key correction: **do not treat a module the
Python side imports but the native list lacks as "missing"** - in every case so
far it was renamed, folded into another module, or intentionally retired. Verify
against the native project (and legacy) before concluding.

### Authoritative native embedded-module list (39)

Py4GW, PySystem, PySettings, PyProfiler, PyCallback, PyListeners,
PyAgent, PyAgentEvents, PyAgentRecolor, PyCamera, PyChat,
PyDXOverlay, PyDialog, PyEffects, PyFriendList, PyGameThread, PyGuild,
PyImGui, PyInventory, PyItem, PyKeystroke, PyListeners, PyMap,
PyMerchant, PyMouse, PyNameObfuscator, PyOverlay, PyPacketSniffer,
PyParty, PyPathing, PyPing, PyPlayer, PyProfiler, PyQuest, PyRender,
PyScanner, PySettings, PySkill, PySkillbar, PySystem, PyTexture,
PyTrade, PyUIManager.

### Reconciliation of the modules Py4GWCoreLib imports

**CORRECTED 2026-07-06:** The original survey was written before the reforged C++
bindings were completed. The native bindings have since been built to **preserve
the legacy class API**. The "API-SHAPE" category below was incorrect; nearly all
modules expose the same classes and methods as the legacy DLL. See each module's
`*_bindings.cpp` for the authoritative surface.

Categories: MATCH (compatible today — class API preserved), NAME (specific symbol
renamed), RELOCATED (functionality moved to a differently-named module),
RETIRED (removed by design).

| Python imports | Native reality | Category | Notes |
|---|---|---|---|
| PyScanner | `scanner_bindings.cpp` (`PyScanner` class) | MATCH | class + static methods preserved |
| PyImGui | `imgui/imgui_bindings.cpp` | MATCH | both function-based; ImGui 1.92.x adapted |
| PyPathing | `pathing_bindings.cpp` (`PathPlanner`, `PathStatus`) | MATCH | classes present |
| PyUIManager | `ui/ui_bindings.cpp` (`UIManager`, `UIFrame`, ...) | MATCH | present |
| PyAgent | `agent/agent_bindings.cpp` | MATCH | **PyAgent(agent_id) class preserved** — `.GetAgentID()`, `.GetName()`, `.GetPrimary()`, `.GetLevel()`, `.GetHP()`, `.GetPos()`, `.GetIsLiving()`, `.GetIsDead()`, `.GetIsMoving()`, `.GetIsAttacking()`, `.GetIsCasting()`, `.GetAllegiance()`, `.GetIsGadget()`, `.GetIsItem()`, static `.GetAgentEncName()`, `.GetTargetId()`, `.GetControlledCharacterId()`, `.GetObservingId()`. Also `Profession` class. Modern free-function surface available alongside. |
| PyPlayer | `player/player_bindings.cpp` | MATCH | **PyPlayer() class preserved** — `.id`, `.agent`, `.target_id`, `.observing_id`, `.account_name`, `.account_email`, `.player_uuid`, `.wins`, `.losses`, `.rating`, `.morale`, `.experience`, `.level`, `.missions_completed`, `.unlocked_maps`, `.GetContext()`, `.SendDialog()`, `.ChangeTarget()`, `.InteractAgent()`, `.CallTarget()`, `.IsAgentIDValid()`, `.GetChatHistory()`, `.RequestChatHistory()`, `.IsChatHistoryReady()`, `.Istyping()`, `.SendChatCommand()`, `.SendChat()`, `.SendWhisper()`, `.SendFakeChat()`, `.SendFakeChatColored()`, `.GetPlayerStatus()`, `.SetPlayerStatus()`. |
| PyParty | `party/party_bindings.cpp` | MATCH | **PyParty() class preserved** — `.players`, `.heroes`, `.henchmen`, `.party_size`, `.GetContext()`, `.AddHero()`, `.KickHero()`, `.KickAllHeroes()`, `.AddHenchman()`, `.KickHenchman()`, `.FlagHero()`, `.FlagAllHeroes()`, `.UnflagHero()`, `.UnflagAllHeroes()`, `.SetHardMode()`, `.ReturnToOutpost()`, `.UseHeroSkill()`, `.SetHeroSkillAIEnabled()`, `.GetPartyContextPtr()`, etc. **Hero(id/name) class preserved** — `.GetID()`, `.GetName()`. **PartyTick class preserved** — `.IsTicked()`, `.SetTicked()`, `.ToggleTicked()`. |
| PyItem | `item/item_bindings.cpp` | MATCH | **ItemModifier**, **PyItemType**, **PyItem class preserved** — `.GetName()`, `.GetInfoString()`, `.RequestName()`, `.IsItemNameReady()`, `.item_id`, etc. Rarity enum present. |
| PyInventory | `item/inventory_bindings.cpp` | MATCH | **Bag(id, name) class preserved** — `.GetItems()`, `.GetSize()`, `.id`, `.name`, `.container_item`, `.items_count`, `.is_inventory_bag`, `.is_storage_bag`, `.is_material_storage`. **PyInventory() class preserved** — `.OpenXunlaiWindow()`, `.GetIsStorageOpen()`, `.PickUpItem()`, `.DropItem()`, `.Salvage()`, `.IdentifyItem()`, `.AcceptSalvageWindow()`, etc. `get_bag(id)` dict snapshots also available. |
| PySkillbar | `skillbar/skillbar_bindings.cpp` | MATCH | **Skillbar() + SkillbarSkill classes migrated** (2026-07-04). Free functions retained alongside. |
| PyMerchant | `merchant/merchant_bindings.cpp` | MATCH | **PyMerchant class preserved** — `.GetMerchantItems()`, `.GetTraderItems()`, `.GetQuotedValue()`, `.GetQuotedItemID()`, `.IsTransactionComplete()`, `.TransactItems()`, etc. |
| PyEffects | `effects/effects_bindings.cpp` | MATCH | **PyEffects(agent_id) class preserved** — `.GetEffects()`, `.GetBuffs()`, `.EffectExists()`, `.BuffExists()`, `.DropBuff()`. **EffectType + BuffType classes preserved**. `get_alcohol_level()`, `drop_buff()`, `get_effects()`, `get_buffs()` also available as module-level functions. |
| PyQuest | `quest/quest_bindings.cpp` | MATCH | **PyQuest class preserved** — static `.GetQuest()`, `.GetQuestLog()`, `.GetQuestLogIds()`, `.RequestQuestInfo()`. **QuestData class preserved** — `.quest_id`, `.log_state`, `.name`, `.description`, `.objectives`, `.is_completed`, etc. |
| PyCamera | `camera/camera_bindings.cpp` | MATCH | **PyCamera class preserved** — `.GetContext()`, `.SetYaw()`, `.SetPitch()`, `.SetCameraPos()`, `.SetLookAtTarget()`, `.SetMaxDist()`, `.SetFieldOfView()`, `.UnlockCam()`, `.GetCameraUnlock()`, `.SetFog()`, `.ForwardMovement()`, `.VerticalMovement()`, `.SideMovement()`, `.RotateMovement()`, `.ComputeCameraPos()`, `.UpdateCameraPos()`. Camera field added to shared memory (2026-07-06). |
| PyKeystroke | `virtual_input/virtual_input_bindings.cpp` | NAME | Python code references `PyScanCodeKeystroke`; Reforged exports `PyKeyHandler`. Also `PyMouse` module exists (previously unnamed). |
| PyOverlay | `overlay/overlay_bindings.cpp` | NAME | `Overlay` class preserved. `Point2D`/`Point3D` intentionally renamed to `Vec2f`/`Vec3f`. |
| PySkill | `skillbar/skill_bindings.cpp` | MATCH | PySkill embedded module built 2026-07-04; skill/type/profession names generated from reforged enums. |
| Py2DRenderer | migrated into `DXOverlay`, bound as `PyDXOverlay` | RELOCATED | Python `import Py2DRenderer` → `import PyDXOverlay` |
| PyCombatEvents | reworked into `listeners/`, bound as `PyAgentEvents` | RELOCATED | Python `import PyCombatEvents` → `import PyAgentEvents` |
| PyPointers | replaced by shared memory (`Pointers_SHMemStruct`); 11/12 getters matched, 1 gap (AreaInfo) | RELOCATED | Dead `import PyPointers` lines must be removed from 11 context files |
| PyDialogCatalog | folded into `PyDialog` (`read_dialog_*` accessors) | RELOCATED | Python import is already guarded try/except |

### Binding-layer implications (revised)

- **The legacy class API is preserved** for all major modules: PyAgent, PyPlayer,
  PyParty, PyInventory, PyItem, PyMerchant, PyEffects, PyQuest, PyCamera,
  PySkillbar. The Python wrapper classes (`Agent.py`, `Player.py`, `Party.py`,
  etc.) should work with **minimal or no changes** — they call `PyAgent.PyAgent(...)`,
  `PyPlayer.PyPlayer()`, `PyParty.PyParty()` which all exist.
- **RELOCATED modules only**: Python imports must be repointed (`Py2DRenderer` →
  PyDXOverlay, `PyCombatEvents` → PyAgentEvents, `PyPointers` → shared memory,
  `PyDialogCatalog` → PyDialog).
- **Console/Game repoints**: `Py4GW.Console.*` → `PySystem.Console.*`,
  `Py4GW.Game.enqueue` → `PyGameThread.enqueue`, `Py4GW.Game.get_shared_memory_name`
  → `PySystem.get_shared_memory_name`, `Py4GW.Game.get_tick_count64` →
  `PySystem.get_tick_count64`.
- PyScanner and PyTrade are registered in kEmbeddedModules (2026-07-06).
- Camera shared memory field added to Pointers_SHMemStruct (2026-07-06).

---

## Layer 2 - Shared memory

There are two independent shared-memory systems in the Python tree. Only one
publishes game-context base pointers.

### System A - System Shared Memory (native-published, reforged)

- Manager: `native_src/ShMem/SysShaMem.py`, singleton `SystemShaMemMgr`
  (instantiated + `.enable()` at import).
- Block name from the native side: `Py4GW.Game.get_shared_memory_name()`
  (`SysShaMem.py:35,56`). Python attaches read-only: `SharedMemory(name=...,
  create=False)` (`SysShaMem.py:69`). It never creates the block - it consumes
  what C++ created.
- Layout: `[SharedMemoryHeader (24B)][AgentArraySHMemStruct][Pointers_SHMemStruct]`.
- `SharedMemoryHeader` (`SysShaMem.py:9`, `_pack_=1`): `version`, `total_size`,
  `sequence` (odd = write in progress), `process_id`, `window_handle` (u64).
- Read path `get_payload` (`SysShaMem.py:96`) is a **seqlock**: reads header,
  skips if `sequence` odd, copies out agent array + pointers with
  `from_buffer_copy`, re-reads header, retries on a torn read (up to 3x). Refreshed
  every `PyCallback.Phase.PreUpdate` at priority 0 (before context consumers run).

Structs (System A):

| Struct | File | Purpose | Context pointers? |
|---|---|---|---|
| `SharedMemoryHeader` | `SysShaMem.py` | seqlock header | no |
| `Pointers_SHMemStruct` | `native_src/ShMem/structs/PointersSSM.py` | **15 game-context base pointers** | YES (all) |
| `AgentSHMemStruct` | `structs/AgentArraySSM.py` | one agent: `ptr` (c_void_p) + `agent_id` | YES (per agent) |
| `AgentRefSHMemStruct` / `AgentRefArraySHMemStruct` | `structs/AgentArraySSM.py` | lightweight id/index ref arrays | no |
| `AgentArraySHMemStruct` | `structs/AgentArraySSM.py` | full array (`AgentArray[300]`) + categorized ref arrays (All/Ally/Enemy/Neutral/Living/Item/Gadget/...) | via entries |
| `AgentArraySHMemWrapper` | `structs/AgentArraySSM.py` | non-ctypes helper (`to_int_list`, `get_ally_array`, `get_agent_by_id`) | n/a |

`Pointers_SHMemStruct` fields (all `c_void_p`): MissionMapContext, WorldMapContext,
GameplayContext, InstanceInfo, MapContext, GameContext, PreGameContext,
WorldContext, CharContext, AgentContext, CinematicContext, GuildContext,
AvailableCharacters, PartyContext, ServerRegionContext.

This is the pointer source the contexts read. `AGENT_ARRAY_MAX_SIZE = 300`.

### System B - Py4GW Shared Memory (legacy multibox, Python-managed)

- Manager: `GlobalCache/SharedMemory.py`, singleton `Py4GWSharedMemoryManager`.
- Block name: `"Py4GW_Shared_Mem"` (`SHMEM_SHARED_MEMORY_FILE_NAME`). Unlike System
  A, Python **creates** it if absent, and reads with a **live** `from_buffer`
  overlay so writes persist.
- Root struct `AllAccounts` (`GlobalCache/shared_memory_src/AllAccounts.py`):
  per-slot tables `Keys[64]`, `AccountData[64]`, `Inbox[64]`, `HeroAIOptions[64]`,
  `Intents[64]`. Contains value snapshots (agent data, inventory, skillbar, buffs,
  attributes, titles, quests, faction, mission, messaging inbox, HeroAI options,
  whiteboard intent locks) produced by each client via `from_context()` methods.
- **No game-context pointers.** This is cross-process multibox coordination, not
  part of the pointer-sourcing migration. Left as-is unless it depends on a changed
  binding (e.g. `from_context()` reading a context that must first be fixed).

### Shared-memory implications

- System A is the migration-critical one: its `Pointers_SHMemStruct` is exactly the
  new pointer source. Confirm the native side actually populates every field and
  that the Python struct layout matches the C++ writer byte-for-byte (order + types).
- The seqlock read is already implemented Python-side; the concern is layout/field
  parity with the C++ publisher, not the read mechanism.

---

## Layer 3 - Native contexts

`native_src/context/*.py` - 17 files. Each defines ctypes Structure(s) and a facade
class with `_update_ptr()` registered on `PyCallback.Phase.PreUpdate`, resolving a
base pointer and doing `cast(ptr, POINTER(<Struct>)).contents`. Only the pointer
source differs.

| Context file | Facade | Base-pointer source TODAY | Needs |
|---|---|---|---|
| WorldContext.py | WorldContext | shmem `SSM.WorldContext` | verify field/layout parity |
| PartyContext.py | PartyContext | shmem `SSM.PartyContext` | " |
| MapContext.py | MapContext | shmem `SSM.MapContext` | " |
| CharContext.py | CharContext | shmem `SSM.CharContext` | " |
| GuildContext.py | GuildContext | shmem `SSM.GuildContext` | " |
| AccAgentContext.py | AccAgentContext | shmem `SSM.AgentContext` | field named `AgentContext` in shmem |
| AvailableCharacterContext.py | AvailableCharacterArray | shmem `SSM.AvailableCharacters` | scan alt commented out |
| WorldMapContext.py | WorldMapContext | shmem `SSM.WorldMapContext` | " |
| MissionMapContext.py | MissionMapContext | shmem `SSM.MissionMapContext` | " |
| PreGameContext.py | PreGameContext | shmem `SSM.PreGameContext` | " |
| ServerRegionContext.py | ServerRegion | shmem `SSM.ServerRegionContext` | scan alt commented out |
| CinematicContext.py | Cinematic | shmem `SSM.CinematicContext` | " |
| GameplayContext.py | GameplayContext | shmem `SSM.GameplayContext` | " |
| TextContext.py | TextParser | shmem `SSM.GameContext` then +0x18 `text_parser` | offset chain off GameContext |
| InstanceInfoContext.py | InstanceInfo | **native pattern scan** `InstanceInfo_GetPtr.read_ptr()` | `SSM.InstanceInfo` exists but is commented out - deviation |
| AgentContext.py | AgentArray | **native pattern scan** `AgentArray_GetPtr.read_ptr()` for array base; shmem agent-array wrapper for `GetAgentByID` | mixed source - not in `Pointers_SHMemStruct` |
| GameContext.py | (none) | struct-only, no resolver | consumed by TextContext (+0x18) |

Findings:

- 13 of 17 contexts already read their base pointer from shared memory
  (`SystemShaMemMgr.get_pointers_struct().<Field>`). The legacy
  `PyPointers.Get*Ptr()` call survives only as a commented-out line next to each.
- 2 contexts deviate and still use a native byte-pattern scan
  (`NativeSymbol.read_ptr()`): `AgentContext` (array base) and `InstanceInfoContext`.
  `InstanceInfo` even ignores a live `SSM.InstanceInfo` field. Decide whether these
  should move to shmem for consistency or remain scan-based.
- `GameContext.py` resolves nothing itself; it is a passive struct whose `+0x18`
  `text_parser` is the base for `TextContext`.
- Residual dead `import PyPointers` / `import PyParty` / `import PyPlayer` at the top
  of context files will raise at import if those modules are absent - a concrete
  early-crash candidate independent of the pointer logic below.

Helpers: `native_src/internals/native_symbol.py` (scan+deref),
`native_src/ShMem/SysShaMem.py` + `native_src/ShMem/structs/PointersSSM.py`.

---

## Suggested attack order (per owner direction, revised 2026-07-06)

1. **Bindings first.** The reforged bindings preserve the legacy class API for all
   major modules (PyAgent, PyPlayer, PyParty, PyInventory, PyItem, PyMerchant,
   PyEffects, PyQuest, PyCamera, PySkillbar). The Python wrapper classes should
   work with minimal changes. Migration tasks:
   - Add imports for new modules: `PySystem`, `PyGameThread`, `PyCallback`,
     `PyAgentEvents`, `PyDXOverlay` (in `Py4GWCoreLib/__init__.py`).
   - Repoint RELOCATED modules: `Py2DRenderer` → `PyDXOverlay`,
     `PyCombatEvents` → `PyAgentEvents`, `PyDialogCatalog` → `PyDialog`.
   - Console/Game repoints: `Py4GW.Console.*` → `PySystem.Console.*`,
     `Py4GW.Game.enqueue` → `PyGameThread.enqueue`,
     `Py4GW.Game.get_shared_memory_name` → `PySystem.get_shared_memory_name`,
     `Py4GW.Game.get_tick_count64` → `PySystem.get_tick_count64`.
   - Remove dead `import PyPointers` from 11 context files.
   - Fix NAME differences (PyKeystroke→PyKeyHandler, Point2D/3D→Vec2f/3f).
2. **Shared memory.** Add `Camera` field to `PointersSSM.py` (matches Reforged
   Native's 16-field Pointers_SHMemStruct). Repoint `SysShaMem.py` name source.
   Resolve the 2 scan-based context deviations (AgentContext, InstanceInfoContext).
3. **Contexts load.** With bindings and pointer source correct, the context ctypes
   overlays materialize; validate field offsets against the native struct layouts.

Open items:
- Whether `AgentContext`/`InstanceInfo` should move fully onto shmem.
- Exact bound surface of `PyDXOverlay` vs the `Py2DRenderer` demand.
