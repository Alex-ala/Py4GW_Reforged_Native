# Py4GW Reforged - Changes

## What this update adds

This update brings Py4GW Reforged from a bare injected core up to a usable
scripting platform: the full Python API surface is back, the UI toolkit is
back, and a set of brand new tools (name obfuscation, name-tag recoloring,
packet sniffing, raycasting, dat reading) are available to scripts.

---

## New Python modules for scripts

All the classic game APIs are available again as Python modules:

- **PyAgent** - look up agents, change target, call target, interact with
  NPCs/items/enemies, move the character.
- **PyPlayer** - player identity, names by login number, player queries.
- **PyMap** - travel to maps/districts, instance info, district/region/
  language queries, cinematic skip, challenge missions, altitude queries.
- **PyChat** - send/receive chat, whispers, chat colors, chat log access.
- **PySkillbar** - skill bar state and skill usage.
- **PyEffects** - active effects and buffs.
- **PyItem** - items and inventory data.
- **PyMerchant** - merchant window interaction.
- **PyTrade** - player trade interaction.
- **PyParty** - party composition, heroes, henchmen.
- **PyGuild** - guild data.
- **PyQuest** - quest log, active quest, quest requests.
- **PyFriendList** - friends, ignores, statuses.
- **PyCamera** - camera position and field of view.
- **PyRender** - render-layer queries.
- **PyGameThread** - run code safely on the game thread.
- **PyKeystroke / PyMouse** - send keyboard and mouse input to the game
  window (background-safe virtual input).

## New tools

- **Name Obfuscator (PyNameObfuscator)** - streamer/privacy tool. Replace any
  player name with a fake one everywhere the game shows it: overhead name
  tags, chat messages, guild roster, guild name/tag, item "customised for"
  labels, mercenary heroes, party search, invites, MOTD, even your own name.
  Each surface can be toggled individually, aliases can be added/removed live
  from Python, and a reverse lookup translates fake names back to real ones
  for tooling.

- **Agent Recolor (PyAgentRecolor)** - recolor agent overhead name tags and
  the target/consider ring. Set a color per agent id or per allegiance
  (ally, enemy, neutral, spirit, minion, NPC), read back the game's own
  computed color, clear rules to restore defaults.

- **Packet Sniffer (PyPacketSniffer)** - capture server-to-client and
  client-to-server packets with timestamps and raw bytes, for research and
  debugging. Off by default; scripts start/stop capture on demand.

- **Raycasting (PyMap.RayCast / RayCastTerrain / RayCastInteractive)** -
  true line-of-sight and collision queries against the live map: terrain,
  walkable props, and interactive objects (doors, gates, chests). Includes
  prop enumeration and full collision-mesh extraction for overlay drawing.

- **GW.dat Reader (GWDatReader)** - read and decode game assets straight out
  of GW.dat, including ArenaNet texture decompression and loading them as
  usable D3D textures (icons, map art, etc.).

- **Ping Monitor (PyPing)** - current/average/min/max ping tracker.

- **Shared Memory** - a versioned shared-memory region that publishes the
  game state (contexts, agent list, runtime pointers) so external tools and