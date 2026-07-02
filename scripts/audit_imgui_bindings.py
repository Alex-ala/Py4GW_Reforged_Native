"""
Audit the PyImGui C++ bindings against the vendored Dear ImGui header.

It answers, for the *script-facing* surface only:

1. Which public `IMGUI_API` names from `third_party/imgui/imgui.h` have a Python
   binding (free function OR bound class/enum method), after snake_case
   normalization?
2. Which are still missing?

Host-owned APIs that scripts must never call (context lifecycle, frame begin/end,
IO event injection, font-atlas building, platform/viewport internals, allocators)
are excluded from the "missing" set, because the C++ host owns them by design.

Usage:
    python scripts/audit_imgui_bindings.py
"""

from __future__ import annotations

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
IMGUI_H = ROOT / "third_party" / "imgui" / "imgui.h"
BINDING_DIRS = [ROOT / "src" / "imgui", ROOT / "src" / "imgui" / "bindings"]

# Dear ImGui CamelCase -> existing project snake names that the simple
# normalizer would miss.
ALIASES = {
    "GetIO": "get_io",
    "GetStyle": "get_style",
    "GetColorU32": "get_color_u32",
    "GetColorU32Vec4": "get_color_u32_vec4",
    "Value": "value_bool",
}

# Host-owned / engine-owned API. Scripts run *inside* a frame the C++ host
# drives, so these are intentionally NOT bound. Listed here so they do not show
# up as "missing" and inflate the gap.
HOST_OWNED = {
    # context + frame lifecycle
    "CreateContext", "DestroyContext", "GetCurrentContext", "SetCurrentContext",
    "NewFrame", "EndFrame", "Render", "GetDrawData", "GetPlatformIO",
    # ini wiring done by host (we expose load/save explicitly where useful)
    "GetAllocatorFunctions", "SetAllocatorFunctions", "MemAlloc", "MemFree",
    "DebugCheckVersionAndDataLayout",
    # multi-viewport / platform windows (viewports are not enabled)
    "UpdatePlatformWindows", "RenderPlatformWindowsDefault", "DestroyPlatformWindows",
    "FindViewportByID", "FindViewportByPlatformHandle",
    # IO event injection (the host's WndProc owns input)
    "AddKeyEvent", "AddKeyAnalogEvent", "AddMousePosEvent", "AddMouseButtonEvent",
    "AddMouseWheelEvent", "AddMouseSourceEvent", "AddMouseViewportEvent",
    "AddFocusEvent", "AddInputCharacter", "AddInputCharacterUTF16",
    "AddInputCharactersUTF8", "SetKeyEventNativeData", "SetAppAcceptingEvents",
    "ClearEventsQueue", "ClearInputKeys", "ClearInputMouse",
    # font atlas building (atlas is host-owned via FontManager)
    "GetTexDataAsAlpha8", "GetTexDataAsRGBA32", "GetFontBaked",
    "GetDrawListSharedData", "SetStateStorage", "GetStateStorage",
}


def camel_to_snake(name: str) -> str:
    s1 = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    s2 = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s1)
    return s2.lower()


def imgui_api_names() -> list[str]:
    text = IMGUI_H.read_text(encoding="utf-8", errors="ignore")
    # only the ImGui:: namespace block (skip ImDrawList/ImFontAtlas member-only API)
    names = re.findall(r"IMGUI_API\s+[^;{}()]+?\s+([A-Za-z_]\w*)\s*\(", text)
    seen, out = set(), []
    for n in names:
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


def bound_names() -> set[str]:
    names: set[str] = set()
    pat = re.compile(r'\.def(?:_property|_property_readonly|_readwrite|_readonly|_static)?\(\s*"([A-Za-z_]\w*)"')
    mdef = re.compile(r'\bm\.def\(\s*"([A-Za-z_]\w*)"')
    for d in BINDING_DIRS:
        for cpp in d.glob("*.cpp"):
            text = cpp.read_text(encoding="utf-8", errors="ignore")
            names.update(pat.findall(text))
            names.update(mdef.findall(text))
    return names


def canonical(api_name: str) -> str:
    return ALIASES.get(api_name, camel_to_snake(api_name))


def main() -> None:
    api = imgui_api_names()
    bound = bound_names()

    covered, missing = [], []
    for name in api:
        if name in HOST_OWNED:
            continue
        cand = canonical(name)
        (covered if cand in bound else missing).append((name, cand))

    considered = len(covered) + len(missing)
    pct = (100.0 * len(covered) / considered) if considered else 0.0
    print(f"Dear ImGui public ImGui:: names : {len(api)}")
    print(f"Bound Python names (all files)  : {len(bound)}")
    print(f"Host-owned (excluded)           : {sum(1 for n in api if n in HOST_OWNED)}")
    print(f"Script-facing considered        : {considered}")
    print(f"Covered                         : {len(covered)}  ({pct:.1f}%)")
    print(f"Missing                         : {len(missing)}")
    print()
    print("Missing (script-facing):")
    for name, cand in sorted(missing):
        print(f"  {name} -> {cand}")


if __name__ == "__main__":
    main()
