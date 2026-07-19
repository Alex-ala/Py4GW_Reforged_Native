# Py4GW Reforged — MinGW cross-compile (Linux → 32-bit Windows DLL for Wine)

The upstream build is MSVC/Windows-only (`cmake --preset vs2022-win32`). This
document describes an **additional** build path: cross-compiling `Py4GW.dll` on
Linux with `i686-w64-mingw32` so it can be injected into Guild Wars running under
Wine. The MSVC path is untouched — every change below is gated behind a
`PY4GW_MINGW` CMake flag or `#ifndef _MSC_VER`.

Established 2026-07-18 (Wine 11.13, GCC 16.1.0). Mirrors the approach previously
used for classic Py4GW (`~/git/Py4GW_cpp_files/SUMMARY.md`), re-derived
for this newer, non-GWCA codebase.

## Why a MinGW build was needed

The prebuilt MSVC `Py4GW.dll` injects under Wine but its `FileScanner` aborts at
startup (`Not enough bytes for a section`). Root cause is a Wine-specific
`VirtualQuery` behaviour (see the FileScanner fix below); fixing it needs a
rebuilt DLL, and MSVC isn't available on the Linux host.

## Build

```bash
# one-time: vendor pybind11 (not committed as a submodule here)
#   third_party/pybind11 must contain pybind11 >= 2.13 (2.14.0.dev1 used).

cmake -B build-mingw -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build build-mingw --parallel "$(nproc)"
i686-w64-mingw32-strip Py4GW.dll        # 255 MB -> ~10 MB
```

Output `Py4GW.dll` lands in the repo root (same as the MSVC build). It links
libgcc/libstdc++/winpthread statically, so it needs only `python313.dll` + stock
Windows/UCRT DLLs at runtime.

## Files added (MinGW-only, ignored by the MSVC build)

| File | Purpose |
|------|---------|
| `mingw-toolchain.cmake` | Cross toolchain: compilers, sysroot, and the 32-bit Python 3.13 headers/import-lib from the `gw_wine` prefix. Pre-seeds every `PYTHON*`/`Python3_*` var so pybind11 never probes a target-arch interpreter. Adds `mingw-shims/` to the include path. |
| `include/mingw_compat.h` | Force-included into every Py4GW/imgui_addons TU (replaces the MSVC `/FIbase/error_handling.h`). Supplies `__except`/`__finally` after libstdc++ turns `__try` into `try`; then includes `base/error_handling.h`. |
| `mingw-shims/*.h` | Case-only header shims (`Windows.h`→`windows.h`, etc.) + a complete DirectXMath (MinGW ships a stub). Copied from the classic-Py4GW port. |
| `src/GW/textures/arenanet_texture_stub.cpp` | Replaces the real `arenanet_texture.cpp` (pure MSVC `__asm`/`__declspec(naked)` ATEX decoder GCC can't compile). Provides an empty `GW::textures::AtexDecompress` — texture decompression is unused by the scanner/pathing/bot APIs. |

## Files modified

### `CMakeLists.txt` — all changes gated behind `PY4GW_MINGW`
- Detect MinGW (`MINGW` or GNU+WIN32) → `PY4GW_MINGW`; `message(STATUS ...)`.
- Enable `-msse2 -mfpmath=sse` (i686 defaults to no SSE; GW needs SSE2 and several
  TUs use `<xmmintrin.h>`/`<emmintrin.h>` always-inline intrinsics).
- `CMAKE_CXX_EXTENSIONS ON` for MinGW (GNU anonymous structs / MSVC-isms).
- Translate the `/FI` force-include to `-include .../mingw_compat.h`.
- System libs: MSVC `*.lib` filenames → MinGW `-l` names (`d3d9`, `d3dcompiler`,
  `dbghelp`, `winmm`, `windowscodecs`, `ole32`, `oleaut32`, `gdiplus`, `comctl32`,
  plus `gdi32 user32 shell32 uuid version imm32 dwmapi psapi`).
- Cross-compile Python: skip `find_package(Python3 ... Development)` and expose the
  toolchain-provided 32-bit headers/lib as an `INTERFACE IMPORTED Python3::Python`.
- Swap `arenanet_texture.cpp` ↔ `arenanet_texture_stub.cpp` (exactly one, to avoid
  a duplicate `AtexDecompress`).
- MinGW link opts: `-static-libgcc -static-libstdc++`, whole-archive static
  `winpthread`, `-Wl,--allow-multiple-definition` (pybind11 embedded-module
  symbols duplicated without a PCH), and `-fpermissive -Wno-attributes`.

### `src/base/file_scanner.cpp` — the actual Wine bug fix (not MinGW-specific)
`FileSize` was taken from `VirtualQuery(...).RegionSize`. Under Wine, VirtualQuery
on a `SEC_IMAGE` mapping returns only the first sub-region (the 0x1000 header
page), not `SizeOfImage`, so every section-bounds check failed. Now walks the
contiguous mapped sub-regions of the view to compute the true image size. Benefits
Windows too (harmless there). *(Left untouched: the pre-existing `Sections->` vs
`Section->` typo in the bounds-check loop — separate latent bug.)*

### `include/GW/context/item.h`
Removed default member initializers from `ItemData` (`= 0`, `= {}`). GCC forbids
anonymous-aggregate members (see `Equipment` union in `agent.h`) whose type has a
non-trivial default constructor. These are game-memory overlay structs, so the
initializers were cosmetic. Size static_assert still holds.

### `include/GW/common/stoc.h`
Added `template<>` to all 121 `Packet<T>::STATIC_HEADER` explicit specializations
(GCC requires it; MSVC accepts the bare out-of-class form). Changed
`JumboMessage`'s from `const` to `template<> constexpr` — constexpr static data
members are implicitly inline in C++17, avoiding ODR violations across TUs.

### `include/base/CrashHandler.h`
Added `#include <cstdint>` (used `uint32_t` transitively via the MSVC PCH).

### `src/GW/events/events.cpp`
Added `#include <atomic>` (used `std::atomic` transitively via the MSVC PCH).

### `src/GW/textures/gw_dat_reader.cpp`
`std::ifstream(const std::wstring&)` is an MSVC extension; wrapped the wide path in
`std::filesystem::path(...)` and added `#include <filesystem>`.

### `src/GW/native_ui/native_ui.cpp`
After `<intrin.h>`, `#ifndef _MSC_VER` maps `_ReturnAddress()` to
`__builtin_return_address(0)` — MinGW's `<intrin.h>` declares `_ReturnAddress` but
emits no linkable body.

## Restoring the original MSVC DLL

The committed MSVC `Py4GW.dll` is overwritten in the repo root by this build
(shared output dir). Recover it with:
`git show 46840ac2c3b3333a548b326c69ea25a03a1d149c > Py4GW.dll`
(or `git checkout -- Py4GW.dll`).
