# GWCA Manager Dependency Map

## Rule Used Here

This file treats dependencies the same way `RenderMgr -> UIMgr` had to be treated:

- `hard`: missing it blocks the manager's core functionality
- `situational`: only needed for a feature path, action path, or helper path

So a manager is still a valid migration start if its missing dependencies are only situational.

## Current Base In Reforged

Already migrated:

- `GameThreadMgr`
- `RenderMgr`
- `CameraMgr`
- `EffectMgr`

Shared prerequisites already migrated for current manager work:

- `GW` context access
- shared skill/effect helper code used by `EffectMgr`

Not migrated:

- `UIMgr`
- `MapMgr`
- `ChatMgr`
- `AgentMgr`
- `ItemMgr`
- `StoCMgr`
- `GuildMgr`
- `PartyMgr`
- `TradeMgr`
- `PlayerMgr`
- `MerchantMgr`
- `SkillbarMgr`
- `FriendListMgr`
- `EventMgr`
- `QuestMgr`

Important:

- current UI work was reverted
- treat `UIMgr` as not migrated

## What You Can Start

Ordered from easiest to start to hardest to start.

### Easiest full/core starts

- `EventMgr`
- `StoCMgr`
- `FriendListMgr`
- `PlayerMgr`
- `QuestMgr`
- `GuildMgr`
- `MapMgr`
- `UIMgr`

### Easiest reduced-slice starts

- `MerchantMgr`
- `TradeMgr`
- `ChatMgr`
- `AgentMgr`
- `ItemMgr`
- `SkillbarMgr`
- `PartyMgr`

### Hardest if you want full parity immediately

- `UIMgr`
- `MapMgr`
- `ChatMgr`
- `MerchantMgr`
- `TradeMgr`
- `AgentMgr`
- `ItemMgr`
- `SkillbarMgr`
- `PartyMgr`

## Migration Path Summary

| Manager | Start now? | First useful slice | Full parity blocker |
|---|---|---|---|
| `EventMgr` | yes | event dispatch | none |
| `StoCMgr` | yes | packet hooks and callbacks | none beyond current base |
| `FriendListMgr` | yes | friend list reads and status hooks | no meaningful manager blocker |
| `PlayerMgr` | yes | player/title/account reads | `SkillbarMgr` only for profession-change path |
| `QuestMgr` | yes | quest log and quest lookup | `UIMgr` only for active/abandon actions |
| `GuildMgr` | yes | guild/faction data reads | `UIMgr` and `MapMgr` only for guild-hall actions |
| `MapMgr` | yes | map/context/state reads | `UIMgr` only for travel and mission-entry actions |
| `UIMgr` | yes | UI frame/message foundation | none beyond current base |
| `MerchantMgr` | reduced only | merchant item reads | `UIMgr` for quote and transact actions |
| `TradeMgr` | reduced only | trade state and offered-item reads | `UIMgr` for trade actions |
| `ChatMgr` | reduced only | chat log, typing state, channel state | `UIMgr` for the real chat behavior surface |
| `AgentMgr` | reduced only | agent arrays, ids, basic lookups | `UIMgr` for actions; full parity reconnects into gameplay cluster |
| `ItemMgr` | reduced only | item arrays, bags, storage, gold, lookups | full parity reconnects into `UIMgr` + gameplay cluster |
| `SkillbarMgr` | reduced only | skill data, attributes, skillbar arrays, templates | full parity reconnects into `UIMgr` + gameplay cluster |
| `PartyMgr` | reduced only | party info, counts, loaded/leader state | full parity reconnects into `UIMgr` + gameplay cluster |

## Manager Read

| Manager | Hard deps for core migration | Situational deps you can activate later | Viable first slice | Read |
|---|---|---|---|---|
| `UIMgr` | `GameThreadMgr`, `RenderMgr` | none that block start | frame lookup, message dispatch, callback registration | safe now; major shared hub |
| `StoCMgr` | `GameThreadMgr` | none | packet registration, packet callbacks, delayed hook init | safe now |
| `EventMgr` | none | none | event registration and dispatch | safe now |
| `FriendListMgr` | none | weak `UIMgr`, weak `EventMgr` | friend list reads, add/remove, friend status hooks | safe now |
| `PlayerMgr` | none | `SkillbarMgr`, weak `UIMgr` | player array, active player, titles, account/player reads | safe now |
| `GuildMgr` | none for core reads | `MapMgr`, `UIMgr` | guild array, current guild, faction/guild data reads | safe now for core guild functionality |
| `QuestMgr` | none for core reads | `UIMgr` | quest log, active quest, quest lookup | safe now for quest data/core reads |
| `MapMgr` | `GameThreadMgr` | `UIMgr` | instance/map context, region, outpost/explorable state, map identity | can start early; travel and enter-mission paths can wait |
| `ChatMgr` | `GameThreadMgr` for init timing; `UIMgr` for most active behavior | none | chat log read, channel mapping, typing state, channel color state | small read/config slice exists now, but most of legacy chat is UI-shaped |
| `MerchantMgr` | none for merchant item reads; `UIMgr` for quote/transact actions | none | merchant item array reads | core reads can start now; transactions still want `UIMgr` |
| `TradeMgr` | none for trade-context reads; `UIMgr` for actions | `ItemMgr` | trade state, offered-item inspection | core reads can start now; active trade actions still want `UIMgr` |
| `AgentMgr` | none for agent/map data reads; `UIMgr` for actions | `MapMgr`, `PartyMgr`, `PlayerMgr`, `ItemMgr` | agent arrays, lookups, target/observing ids, player/npc resolution | core reads can start; action surface belongs to gameplay cluster |
| `ItemMgr` | none for inventory/query core | `UIMgr`, `MapMgr`, `StoCMgr`, `AgentMgr` | item arrays, bags, storage state, item lookup/counting, gold reads | core reads can start, but full manager is cluster work |
| `SkillbarMgr` | `GameThreadMgr` for hook/init timing | `UIMgr`, `MapMgr`, `PartyMgr`, `AgentMgr` | skill data, attribute data, skillbar arrays, template encode/decode | core data can start, but full manager is cluster work |
| `PartyMgr` | `GameThreadMgr` | `UIMgr`, `MapMgr`, `ChatMgr`, `PlayerMgr`, `SkillbarMgr`, `AgentMgr` | party info, counts, loaded/leader/hardmode state, hero metadata | read/state slice is possible; full manager is the most entangled |

## Main Hubs

- `UIMgr` is the biggest shared hub
- `MapMgr` is the next shared world-state hub
- `AgentMgr` is the gameplay hub
- `PartyMgr` is the most entangled follow-on manager

## Migrated Since Baseline

- `EffectMgr` is no longer a candidate manager; it is already migrated
- `EffectMgr` was unblocked by migrating its shared prerequisites first instead of adding local compatibility code
- current baseline now includes manager migrations plus the shared `GW/context` support needed by those migrations

## Why Some Dependencies Are Not Blockers

- `RenderMgr -> UIMgr` was only screenshot gating, so render was still migratable.
- `MapMgr -> UIMgr` is mostly travel, enter-mission, and world-map style actions; map context reads do not need that path first.
- `ChatMgr -> UIMgr` is mostly send/print/whisper/log routing; chat log and typing-state reads do not need that path first.
- `MerchantMgr -> UIMgr` is mostly request-quote and transact-item routing; merchant item reads do not need that path first.
- `TradeMgr -> UIMgr` is mostly open/accept/cancel/offer/remove actions; trade context inspection does not need that path first.
- `AgentMgr -> UIMgr` is mostly dialog, target, call-target, and movement/action routing; agent arrays and lookups are not blocked by that.
- `ItemMgr -> UIMgr` is mostly use/move/salvage/interact actions; inventory and bag reads are not blocked by that.
- `QuestMgr -> UIMgr` is mostly active/abandon quest actions; quest log access is not blocked by that.
- `GuildMgr -> UIMgr` and `GuildMgr -> MapMgr` are mostly guild-hall travel helpers; guild data reads are not blocked by that.
- `FriendListMgr -> UIMgr` and `FriendListMgr -> EventMgr` are weak/commented edges in legacy code, not core blockers.

## Short Answer

If you want the easiest path first, start with:

1. `EventMgr`
2. `StoCMgr`
3. `FriendListMgr`
4. `PlayerMgr`
5. `QuestMgr`
6. `GuildMgr`
7. `MapMgr`
8. `UIMgr`

If you are willing to start with reduced read/core slices instead of full parity, these are also valid starts:

1. `MerchantMgr`
2. `TradeMgr`
3. `ChatMgr`
4. `AgentMgr`
5. `ItemMgr`
6. `SkillbarMgr`
7. `PartyMgr`

Leave these for later only if you want their full action-heavy behavior immediately:

1. `PartyMgr`
2. `SkillbarMgr`
3. `ItemMgr`
4. `AgentMgr`
5. `TradeMgr`
6. `MerchantMgr`
7. `ChatMgr`
8. `MapMgr`
9. `UIMgr`
