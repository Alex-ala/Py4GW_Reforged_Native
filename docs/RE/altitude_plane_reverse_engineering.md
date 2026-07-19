# Map altitude / z-plane system (FindZ, QueryAltitude, planes)

RE of how Guild Wars resolves ground height, and why `Overlay::FindZ` returns the
wrong surface for any point that is not on the player's current plane. Symbols come
from `Gw.wasm` (has C++ names); the shipped logic is the same in `Gw.exe`.

## The problem

`Overlay::findZ(x, y, pz)` ignores `pz` and calls `QueryZ(x, y, player->plane)`.
So it samples the ground **at the player's plane**. That is correct only while the
target point is on the same plane as the player. A fixed anchor on a slope/bridge
reads correctly until the player walks off that plane, at which point `FindZ` snaps
to the base terrain under it. This breaks anything that queries height for points
away from the player (path display, decals, world markers).

## What the engine actually does

`MapQueryAltitude(MapPoint const& point, float radius, float* out_alt, Coord3f* out_normal)`
- WASM: `ram:8087b955`
- This is the function the native layer resolves as `g_query_altitude_func`
  (`offsets/map.json` -> `query_altitude_func`, via the `query_altitude_callsite`
  pattern; the exe callsite sits at `0x70c37d`, call target inside `FUN_0070c310`).

Behaviour (stripping the WASM async/coroutine state-machine noise):

1. If a **PathEngine** is valid, it is consulted first: `PathEngineQueryAltitude`
   (`ram:80863e66`) does a plane-less navmesh grid lookup at (x,y) and writes the
   walkable height (note `*out = -fVar3`, confirming **up = -z**).
2. It always queries the **base terrain mesh**: `TrnQueryAltitudeUndecimated`
   (`ram:807256f7`) — this is **plane 0**.
3. It reads `point.zplane` (3rd field of `MapPoint`). **If `zplane == 0` it returns
   the terrain result.**
4. **If `zplane != 0`** it resolves that plane to a **prop** (`PathGetProp(ctx, zplane, ...)`)
   and queries `PropsGetAltitude` (`ram:806e1c26`), then **combines terrain and prop
   by `min` altitude**. With up = -z, `min` = **topmost** surface.

### Consequences

- **Plane 0 = base terrain. Planes > 0 = props** (bridges, ramps, raised platforms).
  Slopes/overpasses are props on planes > 0.
- `MapQueryAltitude` **trusts the `zplane` you pass** — it never searches for the
  right plane. The "which plane is this entity on" decision is made elsewhere
  (movement / path engine) and stored on the agent (`agent->plane`).
- The engine's own tie-break between overlapping surfaces is **`min` = topmost**.

## Exe addresses (Gw.exe 06-14, image base 0x00400000)

Resolved from the `query_altitude_callsite` pattern: the near call at `0x0070c378`
targets `MapQueryAltitude`.

| Function | Gw.exe | Notes |
|---|---|---|
| `MapQueryAltitude` | `0x007051d0` | = resolved `g_query_altitude_func` |
| `PathEngineQueryAltitude` | `0x00731ae0` | navmesh; `(PathEngine*, Coord2f*, float* out)`; `*out = -height` |
| path-engine present? | `0x00731ad0` | selects path-engine vs terrain+prop branch |
| `TrnQueryAltitudeUndecimated` | `0x00749f20` | terrain mesh (ctx+0x84) |
| `PathGetProp` | `0x0071c790` | prop for a zplane (ctx+0x74) |
| `PropsGetAltitude` | `0x00733560` | prop altitude (ctx+0x7c) |
| min-combine terrain vs prop | `0x004f0cf0` | keeps topmost |

`FUN_00731ae0` decompiles cleanly and matches the WASM: int (x,y) -> vtable[0x48]
grid lookup -> if cell == -1 return 0 (no surface) -> vtable[0x78] height ->
`*out = -height` -> return 1.

## Option B — plane-less walkable height (implemented via plane 0)

Because `MapQueryAltitude` **routes through `PathEngineQueryAltitude` whenever a path
engine exists** (the `0x00731ad0` branch, regardless of plane), querying the resolved
`MapQueryAltitude` at **plane 0** yields the navmesh walkable height in path-engine
maps and base terrain elsewhere — no separate pattern scan or vtable/pointer chasing
needed. Exposed as `Overlay::GroundZWalkable(x,y)` (+ batch). It is **plane-independent**
(stable regardless of the player's plane, unlike `FindZ`), so it is ideal for path
display. It does NOT add props, so it will not raise onto a bridge/slope prop.

## Option A vs B — when each is right

- On a **slope/bridge that is a prop** (plane > 0), only **A (`GroundZTop`)** raises
  onto it; **B (`GroundZWalkable`)** returns the base surface (same limitation as
  `FindZ` off-plane). Confirmed by the anchor test: plane-0/path-engine returned flat
  ground for an off-player slope point.
- For **path display on the walkable navmesh**, **B** is the cheap, correct,
  plane-independent choice (one call, no iteration).
- Ship both; the beacon's anchor panel shows `FindZ` / `Top(A)` / `Walkable(B)` side
  by side so the right one per use-case is obvious in-game.

The raw `PathEngineQueryAltitude` at `0x00731ae0` could be resolved directly for the
navmesh height *even in terrain maps*, but that is redundant with `GroundZWalkable`
in the maps where a path engine is what matters, so it is left unbound for now.

## The fix (implemented) — Option A, plane iteration + topmost

Native, in `overlay.cpp` (reuses the already-resolved `QueryAltitude`; `FindZ` and
`MapQueryAltitude` are untouched):

- `QueryPlaneAltitude(x, y, plane, &out)` — coverage-aware per-plane query. Uses the
  `int` return of `QueryAltitude` (0 = this plane has no surface at (x,y)) so planes
  that don't cover the point are ignored.
- `ResolveTopZ(x, y, planes)` — topmost covered surface (`min` z, mirroring the
  engine). Falls back to plane-0 altitude if nothing covers the point.
- `ResolveNearestZ(x, y, ref_z, planes)` — covered surface nearest a reference z
  (disambiguates bridge-over vs under-bridge; feed the object's own z).

Exposed on `PyOverlay.Overlay`. The topmost resolution is folded **into `FindZ`** as a
`multi_plane` flag (default true) rather than a separate function, so existing call
sites get the fix without a rename:

| Binding | Purpose |
|---|---|
| `FindZ(x, y, pz=0, multi_plane=True)` | ground height; default = topmost across all planes (plane-independent). `multi_plane=False` = legacy player-plane |
| `FindZBatch(points, multi_plane=True)` | batched FindZ (path display, dozens/frame); plane list once, single-plane fast path |
| `GetZPlaneCount()` | pmaps count; **cheap gate** — `<= 1` = single-plane, fast path applies |
| `QueryAltitudeAt(x, y, plane) -> (covered, z)` | altitude at a specific plane (primitive) |
| `GroundZNearest(x, y, ref_z)` (+ `...Batch`) | specialist: nearest-to-ref surface (bridge-over vs under; FindZ's topmost can't) |
| `GroundZWalkable(x, y)` (+ `...Batch`) | Option B: plane-less navmesh/terrain height (see below) |

### Performance notes (FindZ is called dozens of times/frame)

- **Gate on `GetZPlaneCount()`**: most maps are single-plane; skip all iteration.
- **Batch**: `GroundZTopBatch` fetches the plane list once and takes the single-plane
  fast path, so dozens of points cost one Python<->C++ crossing.
- The plane count is fixed per map; cache it per map load, don't re-query per frame.
- For entities (agents/items/gadgets) you often don't need the resolver at all —
  they carry their own `zplane`; query that plane directly.

## Sign convention

`up = -z` in this world: a smaller altitude value renders higher (the overlay draws
with up = -z, and lifting a decal does `z - lift`). Hence **topmost = min z**, which
also matches `MapQueryAltitude`'s internal `min` combine.

## Related altitude symbols (Gw.wasm)

- `MapQueryAltitude(MapPoint const&, float)` `ram:8087b7ab` (2-arg convenience)
- `MapQueryAltitudeTerrain(MapPoint const&, float)` `ram:8087c7e3` (terrain only)
- `PathEngineQueryAltitude(PathEngine*, Coord2f const&, float*)` `ram:80863e66`
- `TrnQueryAltitudeUndecimated / Decimated` `ram:807256f7 / 807212c0`
- `ITerrain::CalcChunkAltitudeUndecimated` `ram:8071b8c6`
- `IProps::IntersectAltitude` `ram:806d626f`, `PropsGetAltitude` `ram:806e1c26`

## Open follow-up (deferred)

If topmost/nearest ever misbehaves on stacked/tunnel geometry, RE the movement path
that assigns `agent->plane` (path engine + `PathGetProp`) to get the game's
authoritative plane for an arbitrary (x,y). Entry points above are enough to start.
