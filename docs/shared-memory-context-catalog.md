# Shared Memory — Context Catalog (categorization worksheet)

Complete inventory of every context struct in `include/GW/context/*.h`, with
sizes. This is the worksheet for deciding, per item, what the C++ side publishes
into shared memory and how:

- **FULL** — copy the whole struct (with values) into the region each frame.
- **PTR** — publish the pointer only; Python materializes it via ctypes (for
  big / variable-length data).
- **SKIP** — not published.

Fill the DECISION column. Sizes are `sizeof` from the C++ static_asserts.
"Big data inside" = the struct owns `GW::GWArray` / pointer members that point
at large or variable-length data (that data stays behind the pointer whether or
not the owning struct is copied FULL).

Legend: **[ROOT]** = has a `GW::Context::Get*()` accessor (a publish root).

---

## 1. Publish roots (have a Get* accessor)

| Context | size | big data inside | DECISION | notes |
|---------|------|-----------------|----------|-------|
| AgentContext        | (var) | agent array (300), movement array | | agent array is the main voluminous one |
| CharContext         | 0x440 | ObserverMatch inline | | player character |
| WorldContext        | 0x854 | attributes, titles, skills, quests, unlocks, party allies, minions | | biggest context; many arrays |
| PartyContext        | 0xD0  | player/hero/henchman arrays, party search | | |
| ItemContext         | 0x10C | bags, inventory, item arrays | | |
| MapContext          | (var) | pathing maps, props, terrain, zones | | |
| GameContext         | 0x5C  | sub-context pointers | | top-level |
| GameplayContext     | 0x78  | — | | |
| GuildContext        | (var) | guild array, players, history | | |
| TradeContext        | 0x38  | trade items array | | |
| AccountContext      | 0x138 | unlocked-item info | | |
| FriendList          | (var) | friends array | | |
| PreGameContext      | 0x100 | login character list | | char select |
| MissionMapContext   | 0x48  | icons | | |
| WorldMapContext     | 0x224 | — | | |
| Camera              | (var) | — | | pos/target/fov |
| TextParser          | 0x1D4 | text cache | | |
| GwDxContext (render)| (var) | — | | D3D device |
| SalvageSessionInfo  | 0x24  | — | | |

## 2. Sub-structs owned by each root (reached via pointers)

These are the data behind the arrays/pointers above. If a root is FULL, these
are reached by Python walking the copied pointers; if a root is PTR, same. Mark
any you want published as their OWN dedicated segment rather than via the parent.

### Agents (agent.h) — under AgentContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| Agent            | 0xC4  | | base agent |
| AgentLiving      | 0x1C4 | | the rich per-agent record (hp/energy/effects/skill/…) |
| AgentItem        | 0xD4  | | |
| AgentGadget      | 0xE4  | | |
| AgentMovement    | 0x80  | | |
| Equipment        | 0xD8  | | |
| TagInfo          | —     | | guild/level tag |
| VisibleEffect    | 0xC   | | per-agent visible effects (TList) |
| AgentEffects     | 0x24  | | |
| AgentInfo        | 0x38  | | |
| AgentSummaryInfo | 0xC   | | |
| MapAgent         | 0x34  | | |
| NPC (npc.h)      | 0x30  | | |

### World (world.h) — under WorldContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| PlayerControlledCharacter | 0x134 | | |
| AccountInfo               | 0x1C  | | |
| PartyAlly                 | 0xC   | | |
| PartyMemberMoraleInfo     | —     | | |
| PartyMoraleLink           | 0xC   | | |
| ProfessionState           | 0x14  | | |
| PetInfo                   | 0x1C  | | |
| ControlledMinions         | 0x8   | | |
| DupeSkill                 | 0x8   | | |
| Player (player.h)         | 0x50  | | |

### Attributes / titles / quests (attribute.h, title.h, quest.h) — under WorldContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| Attribute       | 0x14  | | |
| AttributeInfo   | 0x14  | | |
| PartyAttribute  | 0x43C | | big (party-wide attribute block) |
| Title           | 0x2C  | | |
| TitleTier       | 0xC   | | |
| TitleClientData | —     | | |
| Quest           | 0x34  | | |
| MissionObjective| 0xC   | | |

### Party (party.h) + heroes (hero.h) — under PartyContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| PlayerPartyMember   | 0xC  | | |
| HeroPartyMember     | —    | | |
| HenchmanPartyMember | 0x34 | | |
| PartyInfo           | 0x84 | | |
| PartySearch         | 0x94 | | |
| HeroInfo            | 0x78 | | |
| HeroFlag            | 0x24 | | |

### Items (item.h) — under ItemContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| Inventory          | 0x98  | | |
| Bag                | 0x28  | | |
| Item               | 0x54  | | |
| ItemData           | 0x10  | | |
| ItemModifier       | 0x4   | | |
| ItemFormula        | 0x14  | | |
| MaterialCost       | 0x10  | | |
| WeaponSet          | 0x8   | | |
| DyeInfo            | 0x3   | | |
| CompositeModelInfo | 0x30  | | |
| PvPItemInfo        | 0x24  | | |
| PvPItemUpgradeInfo | 0x28  | | |
| InventoryTableEntry| 0xC   | | |
| ItemClickParam     | 0xC   | | |

### Map / pathing (map.h, pathing.h) — under MapContext / InstanceInfo
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| AreaInfo             | 0x7C | | current map info |
| InstanceInfo         | —    | | |
| MapTypeInstanceInfo  | —    | | |
| MapDimensions        | —    | | |
| PropsContext         | 0x1A4| | |
| MapProp              | 0x90 | | |
| PropModelInfo        | 0x18 | | |
| PropByType           | —    | | |
| RecObject            | 0xC  | | |
| PathingMap           | 0x54 | | |
| PathingTrapezoid     | 0x30 | | |
| Portal / Node / SinkNode / XNode / YNode | 0x14 / 0x8 / 0xC / 0x20 / 0x18 | | pathing graph |
| MissionMapIcon       | 0x28 | | |
| MissionMapSubContext / SubContext2 | — | | |

### Skills / effects (skill.h) — under Skillbar / agents
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| Skillbar               | 0xBC | | |
| SkillbarSkill          | 0x14 | | |
| SkillbarCast           | —    | | |
| Skill                  | 0xA4 | | |
| SkillTemplate          | —    | | |
| SkillTemplateAttribute | —    | | |
| Buff                   | 0x10 | | |
| Effect                 | 0x18 | | |
| AgentEffects           | 0x24 | | |

### Guild (guild.h) — under GuildContext
| struct | size | DECISION | notes |
|--------|------|----------|-------|
| Guild             | 0xAC  | | |
| GuildPlayer       | 0x174 | | |
| GuildHistoryEvent | 0x208 | | big |
| CapeDesign        | 0x1C  | | |
| GHKey             | —     | | |
| TownAlliance      | 0x78  | | |

### Friends / trade / account / char / chat / cinematic / gadget / pregame / text
| struct | size | parent | DECISION | notes |
|--------|------|--------|----------|-------|
| Friend               | —     | FriendList | | |
| FriendEventData      | —     | FriendList | | |
| TradeItem            | 0x8   | TradeContext | | |
| TradePlayer          | 0x14  | TradeContext | | |
| AccountUnlockedCount | 0xC   | AccountContext | | |
| AccountUnlockedItemInfo | 0xC| AccountContext | | |
| CharProgressBar      | 0x2C  | CharContext | | |
| ObserverMatch (match.h) | 0x78 | CharContext | | |
| ChatBuffer / ChatMessage | — | (chat)     | | |
| Cinematic            | —     | CinematicContext | | |
| GadgetContext        | 0x10  | [ROOT?] | | GadgetInfo 0x10 |
| LoginCharacter       | 0x78  | PreGameContext | | |
| TextCache / SubStruct1 / SubStructUnk | 0x4 / — / 0x54 | TextParser | | |

## 3. Snapshot / derived (not a game struct)

| segment | DECISION | notes |
|---------|----------|-------|
| Agent categorized id-lists (ally/enemy/neutral/minion/pet/item/gadget/dead…) | | built by iterating + classifying the agent array each frame; current `AgentArraySnapshot` |
| RuntimePointersSnapshot (all context addresses) | | current `runtime.pointers` segment |

## 4. Excluded by default — `ui.h` (transient UI-message packets)

These are message payloads passed to UI hooks, not persistent context state, so
they are NOT shared-memory candidates. Listed so you can veto the exclusion.
A few in `ui.h` are actual UI *state* (not packets) and could be published:
`WindowPosition`, `AgentNameTagInfo`, `FloatingWindow`, `EnumPreferenceInfo`,
`NumberPreferenceInfo`, `UIChatMessage`.

Packets (excluded): SendCallTarget, MouseCoordsClick, IdentifyItem,
ShowXunlaiChest, MoveItem, Resize, TomeSkillSelection, MeasureContent, SetLayout,
SetAgentProfession, WeaponSwap, WeaponSetChanged, ChangeTarget,
SendLoadSkillTemplate, SetRendererValue, EffectAdd, AgentSpeechBubble,
AgentStartCasting, PreStartSalvage, ServerActiveQuestChanged, PrintChatMessage,
PartyShowConfirmDialog, UIPositionChanged, PreferenceFlag/Value/EnumChanged,
PartySearchInvite, PostProcessingEffect, Logout, KeyAction, MouseClick,
MouseAction, ChatLogLine, WriteToChatLog(+WithSender), PlayerChatMessage,
InteractAgent, SendChangeTarget, GetColor, SendLoadSkillbar, SendPingWeaponSet,
SendMoveItem, Vendor(Window/Quote/Items), SendMerchant(RequestQuote/TransactItem),
SendUseItem, SendChatMessage, LogChatMessage, Recv/StartWhisper, CompassDraw,
ButtonMouseAction, Objective(Add/Complete/Updated), ItemUpdated,
InventorySlotUpdated, SendWorldAction, AllyOrGuildMessage, DialogBody/ButtonInfo,
MerchantQuote/TransactionInfo, MapEntryMessage, ChangeTargetUIMsg, ValueChanged.
