"""
Smoke test for the redesigned PyImGui bindings.

Load + run this in-game (it defines draw()). It exercises the new surface:
  - object-oriented DrawList on the foreground list (game overlay)
  - live ImGuiIO handle (read state)
  - color() packing helper
  - a normal window with widgets
  - DockBuilder availability check

Nothing here touches host-owned frame lifecycle; it only draws inside the
frame the C++ host already started.
"""

import PyImGui as imgui

_state = {"checked": False, "slider": 0.5, "frames": 0}


def draw():
    _state["frames"] += 1
    io = imgui.get_io()

    # ---- overlay drawn straight onto the foreground list (no window needed) ----
    fg = imgui.get_foreground_draw_list()
    white = imgui.color(1.0, 1.0, 1.0, 1.0)
    green = imgui.color(0.2, 1.0, 0.4, 1.0)
    red = imgui.color(1.0, 0.2, 0.2, 0.9)

    fg.add_rect((20, 20), (260, 90), white, rounding=6.0, thickness=2.0)
    fg.add_line((20, 55), (260, 55), green, 1.0)
    fg.add_circle_filled((40, 55), 8.0, red)
    fg.add_text((30, 26), white, "PyImGui redesign — foreground overlay")
    fg.add_text((30, 66), green, f"fps={io.framerate:.0f}  frames={_state['frames']}")

    # ---- a normal window with widgets ----
    if imgui.begin("Redesign Smoke"):
        imgui.text(f"ImGui {imgui.get_version()}")
        imgui.text(f"display: {io.display_size}  mouse: {io.mouse_pos}")
        imgui.text(f"want_capture_mouse: {io.want_capture_mouse}")

        if imgui.button("Click me"):
            _state["checked"] = not _state["checked"]
        _state["checked"] = imgui.checkbox("toggle", _state["checked"])
        _state["slider"] = imgui.slider_float("value", _state["slider"], 0.0, 1.0)

        # confirm DockBuilder + Dir are reachable from Python
        imgui.text(f"DockBuilder: {hasattr(imgui, 'dock_builder_split_node')}  "
                   f"Dir.Left={int(imgui.Dir.Left)}")

        # confirm addon submodules are reachable
        addons = [a for a in ("filebrowser", "hotkey", "markdown", "memory_editor", "anim")
                  if hasattr(imgui, a)]
        imgui.text("addons: " + ", ".join(addons))
        imgui.markdown.render("**Markdown** via `PyImGui.markdown` — [imgui](https://github.com/ocornut/imgui)")
    imgui.end()


def main():
    draw()
