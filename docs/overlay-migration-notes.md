# Overlay Module Migration Notes

Migration of the legacy `py_overlay.h/.cpp` and `py_2d_renderer.h/.cpp` into the
combined `overlay` module (`include/overlay/`, `src/overlay/`). The module lives
outside `GW/` because it is Py4GW-native drawing infrastructure, not a Guild Wars
subsystem. Namespace: `PY4GW::overlay`. No lifecycle wiring is needed: pattern
resolution is lazy (first use), the D3D device is fetched per call from
`GW::render::GetDevice()`, and the classes are instantiated from Python.

## File map (legacy -> migrated)

- `py_overlay.h/.cpp` (class `Overlay`)      -> `overlay/overlay.h`, `src/overlay/overlay.cpp`, `src/overlay/overlay_patterns.cpp`
- `py_overlay.h/.cpp` (class `ScreenOverlay`) -> `overlay/screen_overlay.h`, `src/overlay/screen_overlay.cpp`
- `py_2d_renderer.h/.cpp` (class `Py2DRenderer`) -> `overlay/dx_overlay.h`, `src/overlay/dx_overlay.cpp` (class `DXOverlay`)
- Python bindings -> `src/overlay/overlay_bindings.cpp` (`PyOverlay`), `src/overlay/dx_overlay_bindings.cpp` (`PyDXOverlay`)
- Inline scanner patterns -> `offsets/overlay.json`

## Intentional deviations from legacy (all user-approved)

- **Rename**: legacy `Py2DRenderer` is now `DXOverlay` ("it is a full DirectX
  overlay, not just a 2D renderer"). Python module `Py2DRenderer` is now
  `PyDXOverlay`.
- **Point2D/Point3D dropped**: the module uses the project's `GW::Vec2f` /
  `GW::Vec3f` (bound in `PyOverlay` as `Vec2f`/`Vec3f`, read-write fields).
  Point2D was int-based; coordinates are now floats end to end (removes the
  legacy float->int truncation in pathing geometry and poly points).
- **D3DX removed** (project has no retired DirectX SDK dependency):
  - `D3DXMatrix*` -> DirectXMath (`XMMatrixLookAtRH` etc., same row-major layout);
  - `D3DXCompileShader` -> `D3DCompile` (d3dcompiler.lib, target ps_2_0);
  - `D3DXSaveSurfaceToFileW` -> WIC PNG encoder (`SaveSurfaceToPngWIC`,
    GetRenderTargetData into a sysmem surface, 32bppBGRA frame).
- **Tint fix**: `DrawTexture`, `DrawTexture3D`, `DrawQuadTextured3D` no longer
  force the tint to opaque white; the `int_tint` argument is respected.
- **Camera fix**: `Overlay::WorldToScreen` no longer caches the camera pointer
  in a function-local static (was stale across map loads); fetched per call
  with a null guard.
- **Resolve-once patterns**: legacy `GetScreenToWorld` ran `Scanner::Find` on
  every call; the port resolves `overlay.screen_to_world_point_func` and
  `overlay.ndc_screen_coords_ptr` once via `PY4GW::Patterns::Resolve`
  (offsets/overlay.json) and reuses the map module's already-resolved
  `MapCliQueryIntersection` instead of a duplicate scan.
- **SaveGeometryToFile result fix**: the legacy code overwrote the save HRESULT
  with `state_block->Apply()`, so a failed file save could return `SAVE_OK`;
  the port keeps the two results separate.
- **Dead code dropped**: `GetVec3f`, `GetMapProps`, `ALTITUDE_UNKNOWN`,
  `Py2DRenderer::EnableDepthBuffer`, the unreachable tail of `FindZPlane`, and
  the unused locals in `WorldMapToScreen`.
- **UTF-8 paths**: texture paths are widened with `MultiByteToWideChar` (same
  helper as `texture_bindings.cpp`); the legacy `wstring(begin, end)` byte-copy
  broke non-ASCII paths.
- **TextureManager access**: legacy `Overlay` copy-constructed the
  TextureManager singleton into a member; the migrated TextureManager is
  non-copyable, so calls go through `TextureManager::Instance()` directly.
- **ImGui 1.92 adaptations** (forced by the vendored ImGui version):
  - `ImFont::Scale` no longer exists; `DrawText2D/3D` pass an explicit size to
    `ImDrawList::AddText` / `ImFont::CalcTextSizeA` (same visual result);
  - the `ImGui::Image(tint, border)` overload was removed upstream; the tinted
    `DrawTexture` uses `ImageWithBg`, with the border color mapped to
    `ImGuiCol_Border` + `ImGuiStyleVar_ImageBorderSize` when its alpha > 0
    (the vendored ImGui predates the dedicated `ImGuiCol_ImageBorder` entry).

## Parity notes

- `Overlay::IsMouseClicked` returns `io.MouseDown[button]` (i.e. "is held"),
  exactly like legacy; the misleading name is kept for surface parity.
- `GetMouseWorldPos` is still asynchronous: the raycast runs on the game thread
  via `GW::game_thread::Enqueue`, so the returned value is the previous
  enqueued result (legacy behavior).
- The typo'd legacy names `WorlMapToGamePos` (C++) and `DrawTextureInForegound`
  are kept, and the Python binding still exposes `WorldMapToGamePos`.
- `overlay.json` scan semantics match the legacy calls: the NDC pattern
  dereferences the pointer operand at scan+0x8 (code bytes are immutable, so
  resolve-time deref equals the legacy per-call read); the screen-to-world
  anchor keeps only the 8 strictly-masked legacy bytes.

## Repo fixes exposed by this migration

- `GW::GetNorm(Vec3f)`/`GetNorm(Vec2f)` were declared in
  `GW/common/game_pos.h` but never defined anywhere (nothing had used
  `GW::Normalize` before); they are now defined inline in the header.
- GDI+ under this project's global `WIN32_LEAN_AND_MEAN`/`NOMINMAX` needs
  `<objidl.h>` plus `min`/`max` shims in the `Gdiplus` namespace before
  `<gdiplus.h>` (see `screen_overlay.cpp`).

## Build integration

- New sources are picked up by the existing `src/**/*.cpp` glob; **CMake must
  be re-configured** after this migration (CONFIGURE_DEPENDS usually handles it
  on the next build).
- New link libraries: `d3dcompiler.lib` (inverse shader), `gdiplus.lib` and
  `comctl32.lib` (ScreenOverlay; legacy used `#pragma comment(lib, ...)`).
- `PyOverlay` and `PyDXOverlay` are imported in `python_runtime::Initialize()`.
