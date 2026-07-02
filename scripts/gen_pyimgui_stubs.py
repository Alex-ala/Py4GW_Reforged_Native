"""
Generate scripts/PyImGui.pyi from the live, embedded PyImGui module.

Run this in-game (defines main()). The embedded module only exists inside the
injected process, so stub generation has to happen there. pybind11 emits a
signature line in each callable's __doc__ ("name(args) -> ret"); we harvest
those to produce an accurate, current stub for IDE/type-checker use.
"""

import enum as _enum
import inspect
import os

import PyImGui as imgui


def _sig_lines(doc: str) -> list[str]:
    if not doc:
        return []
    out = []
    for line in doc.splitlines():
        line = line.strip()
        # pybind signature lines look like "name(self: X, a: int = 0) -> bool"
        if "(" in line and ")" in line and "->" in line:
            out.append(line)
    return out


def _emit_callable(name: str, obj, indent: str, self_cls: str | None) -> list[str]:
    sigs = _sig_lines(getattr(obj, "__doc__", "") or "")
    lines: list[str] = []
    if not sigs:
        prefix = "self, " if self_cls else ""
        lines.append(f"{indent}def {name}({prefix}*args, **kwargs) -> Any: ...")
        return lines
    overloaded = len(sigs) > 1
    for sig in sigs:
        # turn "name(args) -> ret" into "def name(args) -> ret: ..."
        body = sig
        if overloaded:
            lines.append(f"{indent}@overload")
        lines.append(f"{indent}def {body}: ...")
    return lines


def generate() -> str:
    out: list[str] = [
        "# Auto-generated from the live PyImGui module by gen_pyimgui_stubs.py.",
        "# Do not edit by hand; re-run the generator in-game to refresh.",
        "from typing import Any, overload",
        "",
    ]

    names = sorted(n for n in dir(imgui) if not n.startswith("__"))
    classes, enums, funcs, consts = [], [], [], []
    for n in names:
        obj = getattr(imgui, n)
        if isinstance(obj, type) and issubclass(obj, _enum.Enum):
            enums.append(n)
        elif isinstance(obj, type):
            classes.append(n)
        elif callable(obj):
            funcs.append(n)
        else:
            consts.append(n)

    for n in enums:
        obj = getattr(imgui, n)
        out.append(f"class {n}:")
        members = [m for m in dir(obj) if not m.startswith("_")]
        if not members:
            out.append("    ...")
        for m in members:
            try:
                out.append(f"    {m}: int")
            except Exception:
                pass
        out.append("")

    for n in classes:
        obj = getattr(imgui, n)
        out.append(f"class {n}:")
        body_start = len(out)
        for m in sorted(dir(obj)):
            if m.startswith("__") and m not in ("__init__",):
                continue
            attr = inspect.getattr_static(obj, m, None)
            if callable(getattr(obj, m, None)) or callable(attr):
                out.extend(_emit_callable(m, getattr(obj, m), "    ", n))
            else:
                out.append(f"    {m}: Any")
        if len(out) == body_start:
            out.append("    ...")
        out.append("")

    for n in funcs:
        out.extend(_emit_callable(n, getattr(imgui, n), "", None))
    out.append("")

    for n in consts:
        out.append(f"{n}: Any")
    out.append("")
    return "\n".join(out)


def main():
    text = generate()
    here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
    path = os.path.join(here, "PyImGui.pyi")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    try:
        from Py4GW import Console
        Console.log("gen_pyimgui_stubs", f"Wrote {path} ({len(text)} bytes)")
    except Exception:
        print(f"Wrote {path}")
