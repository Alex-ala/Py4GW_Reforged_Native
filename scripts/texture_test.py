"""
Texture pipeline test harness.

Visually confirms the migrated `GW::textures` module against the way real
scripts use textures. Mirrors the legacy `skill_texture_test.py`: it resolves a
skill's icon file id and loads that icon out of the live GW.dat archive, plus a
raw dat file id and a file-on-disk texture (loaded through the new WIC path, no
DirectX SDK).

Load it from the in-game console:

    /load scripts/texture_test.py

How this maps to legacy usage
-----------------------------
Legacy scripts build a "gwdat://<file_id>" key from `skill.icon_file_id` (or an
item's model file id) and hand it to `ImGui.DrawTexture(key, w, h)`, which draws
through the 2D renderer overlay. That renderer is migrated later; here we do the
renderer-free ImGui-native equivalent:

    file_id  = PySkillbar.get_skill_icon_file_id(skill_id)   # from the Skill struct
    handle   = PyTexture.get_texture_by_file_id(file_id)     # GWDatReader async decode
    PyImGui.image(handle, (w, h))                            # draw it

Notes
-----
* DAT textures decode asynchronously (CPU decode on the update loop, GPU upload
  on the draw loop): a handle of 0 means "not ready yet / not found". It fills in
  a frame or two after you set the id.
* DAT loading is gated on a fully-loaded, non-cinematic map with the UI drawn, so
  test from inside an outpost or explorable area.
"""

import PyImGui
import PySkillbar
import PyTexture

# Persistent widget state (kept across frames).
STATE = {
    "skill_id": 330,     # default matches the legacy skill_texture_test
    "dat_file_id": 0,    # any raw GW.dat texture file id
    "file_path": "",     # a file on disk, chosen via the file dialog below
    "model_file_id": 0,  # an item/armor model file id for the dyed preview
    "dye_tint": 0,
    "dye": [0, 0, 0, 0],
    "preview_px": 64,
    "browser": None,     # persistent ImGui file browser (created lazily)
}


def _preview(label, handle, size_px):
    if handle:
        PyImGui.text("%s  ->  0x%X (ready)" % (label, handle))
        PyImGui.image(handle, (float(size_px), float(size_px)), (0.0, 0.0), (1.0, 1.0))
    else:
        PyImGui.text("%s  ->  0 (loading / not found / map not ready)" % label)


def draw():
    PyImGui.begin("Texture Test Harness")

    STATE["preview_px"] = PyImGui.input_int("preview size (px)", STATE["preview_px"], 16, 64, 0)
    if STATE["preview_px"] < 16:
        STATE["preview_px"] = 16
    PyImGui.separator()

    # ---- Skill icon from GW.dat (the primary real-world usage) -----------
    PyImGui.text("Skill icon (GW.dat) - the way scripts load skill icons:")
    STATE["skill_id"] = PyImGui.input_int("skill id", STATE["skill_id"], 1, 10, 0)
    if STATE["skill_id"] < 0:
        STATE["skill_id"] = 0

    icon_id = PySkillbar.get_skill_icon_file_id(STATE["skill_id"]) if STATE["skill_id"] > 0 else 0
    hi_id = PySkillbar.get_skill_icon_file_id_hi_res(STATE["skill_id"]) if STATE["skill_id"] > 0 else 0
    PyImGui.text("icon_file_id=%d   hi_res=%d" % (icon_id, hi_id))
    _preview("skill icon", PyTexture.get_texture_by_file_id(icon_id) if icon_id else 0, STATE["preview_px"])
    _preview("skill icon (hi-res)", PyTexture.get_texture_by_file_id(hi_id) if hi_id else 0, STATE["preview_px"])
    PyImGui.separator()

    # ---- Raw GW.dat file id ---------------------------------------------
    PyImGui.text("Raw GW.dat file id:")
    STATE["dat_file_id"] = PyImGui.input_int("dat file id", STATE["dat_file_id"], 1, 100, 0)
    if STATE["dat_file_id"] < 0:
        STATE["dat_file_id"] = 0
    dat_handle = PyTexture.get_texture_by_file_id(STATE["dat_file_id"]) if STATE["dat_file_id"] > 0 else 0
    _preview("dat texture", dat_handle, STATE["preview_px"])
    PyImGui.separator()

    # ---- Dyed / colored model texture (GW.dat) --------------------------
    PyImGui.text("Dyed model texture (GW.dat) - base icon + dye mask:")
    STATE["model_file_id"] = PyImGui.input_int("model file id", STATE["model_file_id"], 1, 100, 0)
    if STATE["model_file_id"] < 0:
        STATE["model_file_id"] = 0
    STATE["dye_tint"] = PyImGui.input_int("dye tint (0..49)", STATE["dye_tint"], 1, 5, 0)
    for i in range(4):
        STATE["dye"][i] = PyImGui.input_int("dye %d (1..13)" % (i + 1), STATE["dye"][i], 1, 1, 0)
    colored_handle = 0
    if STATE["model_file_id"] > 0:
        colored_handle = PyTexture.get_colored_model_texture(
            STATE["model_file_id"], STATE["dye_tint"],
            STATE["dye"][0], STATE["dye"][1], STATE["dye"][2], STATE["dye"][3])
    _preview("colored model", colored_handle, STATE["preview_px"])
    PyImGui.separator()

    # ---- File on disk (WIC), picked with the ImGui file dialog ----------
    PyImGui.text("From file (WIC):")
    fb = PyImGui.filebrowser
    if STATE["browser"] is None:
        STATE["browser"] = fb.FileBrowser(fb.NoModal | fb.CreateNewDir | fb.CloseOnEsc)
        STATE["browser"].set_title("Select a texture")
        STATE["browser"].set_type_filters([".png", ".jpg", ".jpeg", ".bmp", ".gif", ".dds"])
    browser = STATE["browser"]
    if PyImGui.button("Browse for texture..."):
        browser.open()
    browser.display()  # must be called every frame while the dialog is open
    if browser.has_selected():
        STATE["file_path"] = browser.get_selected()
        browser.clear_selected()
    PyImGui.text("selected: " + (STATE["file_path"] if STATE["file_path"] else "(none)"))
    file_handle = PyTexture.get_file_texture(STATE["file_path"]) if STATE["file_path"] else 0
    _preview("file texture", file_handle, STATE["preview_px"])
    PyImGui.separator()

    if PyImGui.button("Cleanup unused textures now"):
        PyTexture.cleanup_old_textures(0)
    PyImGui.text("(DAT icons decode asynchronously - wait a frame or two after changing an id.)")

    PyImGui.end()
