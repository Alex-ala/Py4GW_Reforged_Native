"""
PyImGui feature showcase.

A single, organized demo for the redesigned ImGui bindings. Load it from the
console with:

    /load scripts/imgui_demo.py

Layout
------
Everything lives in one window split into collapsing-header SECTIONS, one per
feature area. Each section is a small `_section_*()` function with comments in
the code and short explanations drawn in the UI itself. A separate foreground
overlay is drawn every frame to demonstrate the screen-space draw list.

It uses the ergonomic `pyimgui` facade (scopes like `with imgui.window(...)`,
the `State` binder, tuple->Vec2 coercion). Anything not wrapped by the facade
falls through to the raw `PyImGui` module automatically.
"""

import pyimgui as imgui


# ── persistent widget state, bound to a State helper (UI.<widget>) ──────────
STATE = {
    "checkbox": False,
    "radio": 0,
    "slider_float": 0.5,
    "slider_int": 50,
    "drag_float": 0.0,
    "input_text": "hello",
    "input_int": 42,
    "input_float": 3.14,
    "hint": "",
    "multiline": "line 1\nline 2",
    "combo": 0,
    "selectable": False,
    "color": [0.3, 0.6, 0.9, 1.0],
    "progress": 0.0,
    # demo bookkeeping
    "anim_on": False,
    "show_metrics": False,
    "overlay": True,
}
UI = imgui.bind(STATE)

# persistent objects for the addon sections (created lazily, kept across frames)
_addons = {"browser": None, "selected_file": "(none)", "mem": None, "mem_editor": None, "hotkeys": None}


def draw():
    # ImAnim keeps per-frame tween state; tick it once before anything tweens.
    if hasattr(imgui, "anim"):
        imgui.anim.update_begin_frame()

    # Foreground overlay: drawn on top of everything, no window required.
    if STATE["overlay"]:
        _overlay()

    # Main window. MenuBar flag lets us host a menu bar inside it.
    with imgui.window("PyImGui - Feature Showcase", flags=imgui.WindowFlags.MenuBar) as win:
        if not win:
            return

        _menu_bar()
        imgui.text_wrapped("Each section below maps to one feature area of the bindings. "
                           "Expand a header to see it; the code for it is in the matching "
                           "_section_* function.")
        UI.checkbox("overlay", "Draw foreground overlay")
        imgui.separator()

        # Each section is gated by a collapsing header so the window stays tidy.
        if imgui.collapsing_header("1. Text and basic widgets"):
            _section_basics()
        if imgui.collapsing_header("2. Sliders, drags and inputs"):
            _section_inputs()
        if imgui.collapsing_header("3. Choice widgets"):
            _section_choices()
        if imgui.collapsing_header("4. Color"):
            _section_color()
        if imgui.collapsing_header("5. Trees and tabs"):
            _section_trees_tabs()
        if imgui.collapsing_header("6. Tables"):
            _section_tables()
        if imgui.collapsing_header("7. Popups, menus, tooltips"):
            _section_popups()
        if imgui.collapsing_header("8. Layout and scopes"):
            _section_layout()
        if imgui.collapsing_header("9. DrawList (canvas + overlay)"):
            _section_drawlist()
        if imgui.collapsing_header("10. Live IO"):
            _section_io()
        if imgui.collapsing_header("11. Style"):
            _section_style()
        if imgui.collapsing_header("12. Docking"):
            _section_docking()
        if imgui.collapsing_header("13. Addons"):
            _section_addons()

    if STATE["show_metrics"]:
        imgui.show_metrics_window()


# ════════════════════════════════════════════════════════════════════════════
#  Sections
# ════════════════════════════════════════════════════════════════════════════

def _menu_bar():
    # menu_bar()/menu() are facade scopes; they only run the body when open.
    with imgui.menu_bar() as bar:
        if not bar:
            return
        with imgui.menu("View") as menu:
            if menu:
                UI.menu_item("show_metrics", "ImGui metrics window")
                UI.menu_item("overlay", "Foreground overlay")


def _section_basics():
    imgui.text("Plain text, bullets, and buttons.")
    imgui.bullet_text("bullet_text draws a leading bullet")
    imgui.text("Hover the next line for a tooltip:")
    imgui.text("  -> hover me <-")
    imgui.show_tooltip("show_tooltip() only draws when the previous item is hovered.")

    if imgui.button("Click"):
        STATE["checkbox"] = not STATE["checkbox"]      # buttons return True on click
    imgui.same_line()                                   # keep the next item on the same row
    UI.checkbox("checkbox", "checkbox bound to State")
    imgui.text(f"checkbox = {STATE['checkbox']}")

    imgui.separator_text("Radio group (one int, several buttons)")
    for idx, label in enumerate(("Low", "Medium", "High")):
        UI.radio("radio", label, idx)
        imgui.same_line()
    imgui.text(f"  selected = {STATE['radio']}")


def _section_inputs():
    # The State binder reads the current value, draws the widget, and writes back.
    UI.slider_float("slider_float", "slider_float", 0.0, 1.0)
    UI.slider_int("slider_int", "slider_int", 0, 100)
    UI.drag_float("drag_float", "drag_float", 0.01, -10.0, 10.0)
    imgui.separator_text("Text / number inputs")
    UI.input_text("input_text", "input_text")
    UI.input_text_with_hint("hint", "with_hint", "type here...")
    UI.input_text_multiline("multiline", "multiline", size=(0, 60))
    UI.input_int("input_int", "input_int")
    UI.input_float("input_float", "input_float")

    # Animated progress bar (value advances each frame).
    STATE["progress"] = (STATE["progress"] + 0.004) % 1.0
    imgui.progress_bar(STATE["progress"], -1, 0, f"{STATE['progress']:.0%}")


def _section_choices():
    # Combo from a Python list (State binder stores the chosen index).
    UI.combo("combo", "combo", ["Apple", "Banana", "Cherry", "Date"])
    imgui.text(f"  index = {STATE['combo']}")

    # Manual combo using the facade scope when you need custom rows.
    with imgui.combo_box("combo_box", "pick one") as open_:
        if open_:
            for idx, item in enumerate(("Foo", "Bar", "Baz")):
                if imgui.selectable(item, STATE["combo"] == idx):
                    STATE["combo"] = idx

    UI.selectable("selectable", "selectable toggle")
    imgui.text(f"  selectable = {STATE['selectable']}")


def _section_color():
    UI.color_edit4("color", "color_edit4")
    r, g, b, a = STATE["color"]
    if hasattr(imgui, "color_button"):
        imgui.color_button("preview", STATE["color"], 0, 40, 20)
        imgui.same_line()
    # color() packs normalized floats into the ImU32 the draw list expects.
    imgui.text(f"packed = {imgui.color(r, g, b, a):#010x}")


def _section_trees_tabs():
    imgui.separator_text("Tree")
    if imgui.tree_node("Expandable node"):
        imgui.text("child item A")
        imgui.text("child item B")
        imgui.tree_pop()

    imgui.separator_text("Tabs")
    with imgui.tab_bar("ShowcaseTabs") as bar:
        if bar:
            with imgui.tab_item("First") as t:
                if t:
                    UI.checkbox("checkbox", "works inside a tab")
            with imgui.tab_item("Second") as t:
                if t:
                    UI.slider_int("slider_int", "also works", 0, 100)


def _section_tables():
    imgui.text("3-column sortable-looking table:")
    flags = imgui.TableFlags.Borders | imgui.TableFlags.RowBg | imgui.TableFlags.Resizable
    with imgui.table("ShowcaseTable", 3, flags=flags) as table:
        if table:
            imgui.table_setup_column("Name")
            imgui.table_setup_column("Value")
            imgui.table_setup_column("Status")
            imgui.table_headers_row()
            for name, value, status in (("Alpha", 12, "OK"), ("Beta", 45, "WARN"), ("Gamma", 78, "OK")):
                imgui.table_next_row()
                imgui.table_set_column_index(0)
                imgui.text(name)
                imgui.table_next_column()
                imgui.text(str(value))
                imgui.table_next_column()
                imgui.text(status)


def _section_popups():
    if imgui.button("Open popup"):
        imgui.open_popup("demo_popup")          # open by id, then draw it below
    with imgui.popup("demo_popup") as popup:
        if popup:
            imgui.text("Popup body")
            imgui.menu_item("Action 1")
            imgui.menu_item("Action 2")

    imgui.text("Right-click this window for a context menu.")
    with imgui.popup_context_window("ctx", 1) as ctx:
        if ctx:
            imgui.menu_item("Context action")


def _section_layout():
    imgui.text("same_line / group / indent keep items arranged.")
    imgui.text("A")
    imgui.same_line()
    imgui.text("B (same line)")

    imgui.separator_text("Scoped style (push/pop handled by the 'with' block)")
    with imgui.style_color(imgui.ImGuiCol.Text, (1.0, 0.7, 0.2, 1.0)):
        imgui.text("this text is orange only inside the scope")
    with imgui.style_var(imgui.ImGuiStyleVar.Alpha, 0.5):
        imgui.text("this text is half-transparent inside the scope")

    imgui.separator_text("Scoped item width + disabled")
    with imgui.item_width(140):
        UI.slider_float("slider_float", "narrow slider", 0.0, 1.0)
    with imgui.disabled(True):
        imgui.button("disabled button")

    imgui.separator_text("Child region + clip rect")
    with imgui.child("layout_child", size=(0, 60)) as child:
        if child:
            for i in range(8):
                imgui.text(f"scrollable child line {i}")


def _section_drawlist():
    imgui.text_wrapped("DrawList is a real object now. get_window_draw_list() draws inside "
                       "this window; get_foreground_draw_list() draws over everything (toggle at top).")

    # Reserve a canvas area and draw into the window's draw list in screen space.
    dl = imgui.get_window_draw_list()
    origin = imgui.get_cursor_screen_pos()      # top-left of the canvas, in screen coords
    ox, oy = origin.x, origin.y
    w, h = 360.0, 160.0

    white = imgui.color(1, 1, 1)
    blue = imgui.color(0.2, 0.4, 0.9)
    red = imgui.color(0.9, 0.2, 0.2)
    green = imgui.color(0.2, 0.9, 0.4)
    yellow = imgui.color(1, 1, 0)

    dl.add_rect_filled((ox, oy), (ox + w, oy + h), imgui.color(0.08, 0.08, 0.10), 6.0)  # backdrop
    dl.add_rect((ox, oy), (ox + w, oy + h), white, 6.0, 1.5)                            # border
    dl.add_line((ox + 10, oy + 30), (ox + 150, oy + 30), green, 2.0)
    dl.add_circle_filled((ox + 60, oy + 90), 28.0, red)
    dl.add_circle((ox + 60, oy + 90), 34.0, yellow, 48, 2.0)
    dl.add_triangle((ox + 150, oy + 120), (ox + 210, oy + 60), (ox + 270, oy + 120), blue, 2.0)
    dl.add_text((ox + 12, oy + 8), white, "window draw list")

    imgui.dummy(w, h)   # advance the cursor past the canvas we just drew


def _section_io():
    # get_io() is a LIVE handle: reads are current, and the safe subset is writable.
    io = imgui.get_io()
    imgui.text(f"display_size = {io.display_size}")
    imgui.text(f"mouse_pos    = {io.mouse_pos}")
    imgui.text(f"framerate    = {io.framerate:.1f} fps")
    imgui.text(f"want_capture_mouse = {io.want_capture_mouse}")
    imgui.text(f"active windows     = {io.metrics_active_windows}")
    # writable example (safe subset): toggle the software cursor
    if imgui.button("toggle mouse_draw_cursor"):
        io.mouse_draw_cursor = not io.mouse_draw_cursor


def _section_style():
    # get_style() returns the live ImGuiStyle by reference; edits apply at once.
    style = imgui.get_style()
    imgui.text(f"WindowRounding = {style.WindowRounding:.1f}")
    if imgui.button("rounder windows"):
        style.WindowRounding = min(style.WindowRounding + 1.0, 12.0)
    imgui.same_line()
    if imgui.button("sharper windows"):
        style.WindowRounding = max(style.WindowRounding - 1.0, 0.0)


def _section_docking():
    imgui.text_wrapped("Docking is enabled by the host. Windows can be dragged onto each other. "
                       "dock_builder_* lets you author layouts in code; Dir.Left etc. choose the split side.")
    imgui.text(f"this window docked: {imgui.is_window_docked()}")
    imgui.text(f"DockBuilder available: {hasattr(imgui, 'dock_builder_split_node')}  "
               f"Dir.Left = {int(imgui.Dir.Left)}")


def _section_addons():
    if not hasattr(imgui, "markdown"):
        imgui.text("Addon submodules are not present in this build.")
        return

    # -- markdown --
    imgui.separator_text("markdown")
    imgui.markdown.render("**markdown** renders *inline* formatting and "
                          "[links](https://github.com/ocornut/imgui).")

    # -- filebrowser --
    imgui.separator_text("filebrowser")
    if _addons["browser"] is None:
        fb = imgui.filebrowser
        _addons["browser"] = fb.FileBrowser(fb.NoModal | fb.CreateNewDir)
        _addons["browser"].set_title("Pick a file")
        _addons["browser"].set_type_filters([".py", ".json", ".txt"])
    browser = _addons["browser"]
    if imgui.button("Open file browser"):
        browser.open()
    browser.display()                       # must be called every frame
    if browser.has_selected():
        _addons["selected_file"] = browser.get_selected()
        browser.clear_selected()
    imgui.text(f"selected: {_addons['selected_file']}")

    # -- memory_editor --
    imgui.separator_text("memory_editor")
    if _addons["mem"] is None:
        _addons["mem"] = bytearray(range(64))          # editable buffer
        _addons["mem_editor"] = imgui.memory_editor.MemoryEditor()
        _addons["mem_editor"].read_only = False
    _addons["mem_editor"].draw_contents(_addons["mem"], 0x1000)

    # -- hotkey --
    imgui.separator_text("hotkey")
    if _addons["hotkeys"] is None:
        hk = imgui.hotkey
        _addons["hotkeys"] = [
            hk.HotKey("Toggle overlay", "demo", 0),
            hk.HotKey("Quick action", "demo", 0),
        ]
    if imgui.button("Edit hotkeys"):
        imgui.open_popup("HotKeys Editor")
    imgui.hotkey.edit(_addons["hotkeys"], "HotKeys Editor")   # opens the modal popup
    for hkk in _addons["hotkeys"]:
        imgui.bullet_text(f"{hkk.name}: {imgui.hotkey.key_lib(hkk.keys) or '(unbound)'}")

    # -- anim --
    imgui.separator_text("anim")
    UI.checkbox("anim_on", "animate toward target")
    target = 1.0 if STATE["anim_on"] else 0.0
    value = imgui.anim.tween_float(
        imgui.get_id("demo_anim"), 0, target, 0.5,
        imgui.anim.Ease.OutCubic, imgui.anim.Policy.Crossfade,
        imgui.get_io().delta_time,
    )
    imgui.progress_bar(value, -1, 0, f"tween {value:.2f}")


# ════════════════════════════════════════════════════════════════════════════
#  Foreground overlay (screen-space, drawn over every window)
# ════════════════════════════════════════════════════════════════════════════

def _overlay():
    fg = imgui.get_foreground_draw_list()
    io = imgui.get_io()
    white = imgui.color(1, 1, 1)
    accent = imgui.color(0.2, 1.0, 0.5)
    fg.add_rect((16, 16), (250, 64), white, 6.0, 1.5)
    fg.add_text((26, 22), white, "PyImGui foreground overlay")
    fg.add_text((26, 42), accent, f"fps {io.framerate:.0f}  mouse {io.mouse_pos}")


def update():
    pass


def main():
    draw()
