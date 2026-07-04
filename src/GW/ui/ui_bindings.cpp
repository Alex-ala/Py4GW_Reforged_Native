#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/ui/ui.h"

#include <string>
#include <vector>

namespace py = pybind11;

// PyUIManager, migrated from legacy py_ui.cpp/py_ui.h. This binds the portion
// of the legacy UIManager surface that maps onto the migrated GW::ui native
// module (frame tree, geometry, preferences, UI messages, typed widget
// families, enc-string helpers). The legacy custom subsystems that carried
// their own scanner-resolved procs/hooks/globals (window clones, devtext
// hosting, window/dialog title hooks, frame logs, safe-destroy input
// scrubbing, frame-list control swarm, key mappings) are NOT ported yet; see
// docs/legacy-migration-checklist.md.
namespace {

using GW::ui::Frame;
using UIMessage = GW::ui::UIMessage;

template <typename T>
T* FrameAs(uint32_t frame_id) {
    return static_cast<T*>(GW::ui::GetFrameById(frame_id));
}

std::wstring SafeWide(const wchar_t* text) {
    return text ? std::wstring(text) : std::wstring();
}

std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr) {
        return {};
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(len), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    if (written <= 0) {
        return {};
    }
    out.resize(static_cast<size_t>(written - 1));
    return out;
}

// Ordered child walk (legacy GetItemFrameID): first child, then next siblings.
uint32_t GetOrderedChildFrameId(uint32_t parent_frame_id, uint32_t index) {
    Frame* cur = GW::ui::GetRelatedFrameById(parent_frame_id, GW::Constants::FrameChild::FirstChild, 0);
    for (uint32_t i = 0; i < index && cur; ++i) {
        cur = GW::ui::GetRelatedFrameById(parent_frame_id, GW::Constants::FrameChild::NextSibling, cur->frame_id);
    }
    return cur ? cur->frame_id : 0;
}

// Legacy UIManager::SendUIMessage packed the value list into a zeroed 16-word
// POD used as wparam.
bool SendUIMessagePacked(uint32_t msgid, std::vector<uint32_t> values, bool skip_hooks) {
    struct UIPayload_POD {
        uint32_t words[16]; // 64 bytes max
    };

    UIPayload_POD payload{};
    const size_t size = values.size();
    for (size_t i = 0; i < size && i < 16; ++i) {
        payload.words[i] = values[i];
    }

    return GW::ui::SendUIMessage(static_cast<UIMessage>(msgid), &payload, nullptr, skip_hooks);
}

py::dict FrameSnapshot(uint32_t frame_id) {
    py::dict out;
    out["frame_id"] = frame_id;
    Frame* frame = GW::ui::GetFrameById(frame_id);
    if (!frame) {
        out["is_created"] = false;
        out["is_visible"] = false;
        return out;
    }

    Frame* parent = frame->relation.GetParent();
    out["parent_id"] = parent ? parent->frame_id : 0;
    out["frame_hash"] = frame->relation.frame_hash_id;
    out["frame_layout"] = frame->frame_layout;
    out["visibility_flags"] = frame->visibility_flags;
    out["type"] = frame->type;
    out["template_type"] = frame->template_type;
    out["child_offset_id"] = frame->child_offset_id;
    out["frame_state"] = frame->frame_state;
    out["is_created"] = frame->IsCreated();
    out["is_visible"] = frame->IsVisible();
    out["is_disabled"] = frame->IsDisabled();

    py::dict pos;
    pos["top"] = frame->position.top;
    pos["left"] = frame->position.left;
    pos["bottom"] = frame->position.bottom;
    pos["right"] = frame->position.right;
    pos["content_top"] = frame->position.content_top;
    pos["content_left"] = frame->position.content_left;
    pos["content_bottom"] = frame->position.content_bottom;
    pos["content_right"] = frame->position.content_right;
    pos["unknown"] = frame->position.unk;
    pos["scale_factor"] = frame->position.scale_factor;
    pos["viewport_width"] = frame->position.viewport_width;
    pos["viewport_height"] = frame->position.viewport_height;
    pos["screen_top"] = frame->position.screen_top;
    pos["screen_left"] = frame->position.screen_left;
    pos["screen_bottom"] = frame->position.screen_bottom;
    pos["screen_right"] = frame->position.screen_right;

    if (Frame* root = GW::ui::GetRootFrame()) {
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        const auto size = frame->position.GetSizeOnScreen(root);
        const auto scale = frame->position.GetViewportScale(root);
        pos["top_on_screen"] = top_left.y;
        pos["left_on_screen"] = top_left.x;
        pos["bottom_on_screen"] = bottom_right.y;
        pos["right_on_screen"] = bottom_right.x;
        pos["width_on_screen"] = size.x;
        pos["height_on_screen"] = size.y;
        pos["viewport_scale_x"] = scale.x;
        pos["viewport_scale_y"] = scale.y;
    }
    out["position"] = pos;

    py::list siblings;
    for (auto it = frame->relation.siblings.begin(); it != frame->relation.siblings.end(); ++it) {
        GW::ui::FrameRelation& sibling = *it;
        Frame* sibling_frame = sibling.GetFrame();
        if (sibling_frame) {
            siblings.append(sibling_frame->frame_id);
        }
    }
    py::dict relation;
    relation["parent_id"] = parent ? parent->frame_id : 0;
    relation["field67_0x124"] = frame->relation.field67_0x124;
    relation["field68_0x128"] = frame->relation.field68_0x128;
    relation["frame_hash_id"] = frame->relation.frame_hash_id;
    relation["siblings"] = siblings;
    out["relation"] = relation;

    return out;
}

}  // namespace

namespace {
struct UIManagerShim {};
}  // namespace

PYBIND11_EMBEDDED_MODULE(PyUIManager, m) {
    m.doc() = "UI manager bindings over the migrated GW::ui native module. "
              "Legacy UIManager subsystems with custom hooks (window clones, "
              "devtext, title hooks, frame logs, control swarm) are not yet ported.";

    py::class_<UIManagerShim>(m, "UIManager")
        // ---- Global state / language ----
        .def_static("get_text_language", []() { return static_cast<uint32_t>(GW::ui::GetTextLanguage()); })
        .def_static("is_world_map_showing", []() { return GW::ui::GetIsWorldMapShowing(); })
        .def_static("is_ui_drawn", []() { return GW::ui::GetIsUIDrawn(); })
        .def_static("is_shift_screenshot", []() { return GW::ui::GetIsShiftScreenShot(); })
        .def_static("set_open_links", [](bool toggle) { GW::ui::SetOpenLinks(toggle); }, py::arg("toggle"))
        .def_static("get_frame_limit", []() { return GW::ui::GetFrameLimit(); })
        .def_static("set_frame_limit", [](uint32_t value) { return GW::ui::SetFrameLimit(value); }, py::arg("value"))

        // ---- Frame tree traversal / discovery ----
        .def_static("get_root_frame_id", []() -> uint32_t {
            Frame* root = GW::ui::GetRootFrame();
            return root ? root->frame_id : 0;
        })
        .def_static("get_frame_array", []() { return GW::ui::GetFrameArray(); })
        .def_static("get_frame_id_by_label", [](const std::wstring& label) {
            return GW::ui::GetFrameIDByLabel(label.c_str());
        }, py::arg("label"))
        .def_static("get_frame_id_by_hash", [](uint32_t hash) { return GW::ui::GetFrameIDByHash(hash); }, py::arg("hash"))
        .def_static("get_hash_by_label", [](const std::wstring& label) {
            return GW::ui::GetHashByLabel(label.c_str());
        }, py::arg("label"))
        .def_static("get_child_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t child_offset) -> uint32_t {
            Frame* child = GW::ui::GetChildFrame(GW::ui::GetFrameById(parent_frame_id), child_offset);
            return child ? child->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("child_offset"))
        .def_static("get_child_frame_path_by_frame_id", [](uint32_t parent_frame_id, std::vector<uint32_t> child_offsets) -> uint32_t {
            Frame* frame = GW::ui::GetFrameById(parent_frame_id);
            for (uint32_t offset : child_offsets) {
                if (!frame) return 0;
                frame = GW::ui::GetChildFrame(frame, offset);
            }
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("child_offsets"))
        .def_static("get_child_frame_id", [](uint32_t parent_hash, std::vector<uint32_t> child_offsets) {
            return GW::ui::GetChildFrameID(parent_hash, std::move(child_offsets));
        }, py::arg("parent_hash"), py::arg("child_offsets"))
        .def_static("get_child_frame_id_from_name_hash", [](uint32_t parent_frame_id, uint32_t name_hash) -> uint32_t {
            Frame* child = GW::ui::GetChildFromNameHash(GW::ui::GetFrameById(parent_frame_id), name_hash);
            return child ? child->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("name_hash"))
        .def_static("get_parent_frame_id", [](uint32_t frame_id) -> uint32_t {
            Frame* parent = GW::ui::GetParentFrame(GW::ui::GetFrameById(frame_id));
            return parent ? parent->frame_id : 0;
        }, py::arg("frame_id"))
        .def_static("get_parent_frame_id_direct", [](uint32_t frame_id) {
            return GW::ui::GetParentFrameId(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("get_related_frame_id", [](uint32_t frame_id, uint32_t relation_kind, uint32_t start_after) -> uint32_t {
            Frame* frame = GW::ui::GetRelatedFrameById(frame_id, static_cast<GW::Constants::FrameChild>(relation_kind), start_after);
            return frame ? frame->frame_id : 0;
        }, py::arg("frame_id"), py::arg("relation_kind"), py::arg("start_after") = 0,
            "Traverses the frame tree by relation kind: 0=first child, 1=last child, 2=next sibling, 3=prev sibling.")
        .def_static("get_first_child_frame_id", [](uint32_t parent_frame_id) -> uint32_t {
            Frame* frame = GW::ui::GetRelatedFrameById(parent_frame_id, GW::Constants::FrameChild::FirstChild, 0);
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"))
        .def_static("get_last_child_frame_id", [](uint32_t parent_frame_id) -> uint32_t {
            Frame* frame = GW::ui::GetRelatedFrameById(parent_frame_id, GW::Constants::FrameChild::LastChild, 0);
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"))
        .def_static("get_next_child_frame_id", [](uint32_t frame_id) -> uint32_t {
            Frame* frame = GW::ui::GetFrameById(frame_id);
            Frame* parent = frame ? GW::ui::GetParentFrame(frame) : nullptr;
            Frame* next = parent ? GW::ui::GetRelatedFrame(parent, GW::Constants::FrameChild::NextSibling, frame) : nullptr;
            return next ? next->frame_id : 0;
        }, py::arg("frame_id"))
        .def_static("get_prev_child_frame_id", [](uint32_t frame_id) -> uint32_t {
            Frame* frame = GW::ui::GetFrameById(frame_id);
            Frame* parent = frame ? GW::ui::GetParentFrame(frame) : nullptr;
            Frame* prev = parent ? GW::ui::GetRelatedFrame(parent, GW::Constants::FrameChild::PrevSibling, frame) : nullptr;
            return prev ? prev->frame_id : 0;
        }, py::arg("frame_id"))
        .def_static("get_item_frame_id", &GetOrderedChildFrameId, py::arg("parent_frame_id"), py::arg("index"))
        .def_static("get_overlay_frame_ids", []() { return GW::ui::GetOverlayFrames(); })
        .def_static("get_popup_frame_ids", []() { return GW::ui::GetPopupFrames(); })
        .def_static("get_frame_hierarchy", []() { return GW::ui::GetFrameHierarchy(); })
        .def_static("get_frame_coords_by_hash", [](uint32_t frame_hash) { return GW::ui::GetFrameCoordsByHash(frame_hash); }, py::arg("frame_hash"))
        .def_static("is_ancestor_of_by_frame_id", [](uint32_t frame_id, uint32_t ancestor_id) {
            return GW::ui::IsAncestorOf(GW::ui::GetFrameById(frame_id), GW::ui::GetFrameById(ancestor_id));
        }, py::arg("frame_id"), py::arg("ancestor_id"))
        .def_static("frame_exists_by_frame_id", [](uint32_t frame_id) {
            Frame* frame = GW::ui::GetFrameById(frame_id);
            return frame && frame->IsCreated();
        }, py::arg("frame_id"))
        .def_static("get_frame_snapshot", &FrameSnapshot, py::arg("frame_id"),
            "Snapshot of a live frame (replaces the legacy UIFrame class): dict "
            "with named fields, position (incl. on-screen coords) and relation.")

        // ---- Frame metadata / geometry ----
        .def_static("get_frame_context", [](uint32_t frame_id) {
            return reinterpret_cast<uintptr_t>(GW::ui::GetFrameContext(GW::ui::GetFrameById(frame_id)));
        }, py::arg("frame_id"))
        .def_static("get_frame_layer_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::GetFrameLayer(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("set_frame_layer_by_frame_id", [](uint32_t frame_id, uint32_t layer) {
            return GW::ui::SetFrameLayer(GW::ui::GetFrameById(frame_id), layer);
        }, py::arg("frame_id"), py::arg("layer"))
        .def_static("get_frame_code_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::GetFrameCode(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("get_frame_min_size_by_frame_id", [](uint32_t frame_id) {
            float w = 0.f, h = 0.f;
            GW::ui::GetFrameMinSize(GW::ui::GetFrameById(frame_id), &w, &h);
            return py::make_tuple(w, h);
        }, py::arg("frame_id"))
        .def_static("get_frame_client_border_by_frame_id", [](uint32_t frame_id) {
            float l = 0.f, t = 0.f, r = 0.f, b = 0.f;
            GW::ui::GetFrameClientBorder(GW::ui::GetFrameById(frame_id), &l, &t, &r, &b);
            return py::make_tuple(l, t, r, b);
        }, py::arg("frame_id"))
        .def_static("get_frame_clip_rect_by_frame_id", [](uint32_t frame_id) {
            float l = 0.f, t = 0.f, r = 0.f, b = 0.f;
            GW::ui::GetFrameClipRect(GW::ui::GetFrameById(frame_id), &l, &t, &r, &b);
            return py::make_tuple(l, t, r, b);
        }, py::arg("frame_id"))
        .def_static("get_frame_position_ex_by_frame_id", [](uint32_t frame_id) {
            float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
            uint32_t flags = 0;
            GW::ui::GetFramePositionEx(GW::ui::GetFrameById(frame_id), &x, &y, &w, &h, &flags);
            return py::make_tuple(x, y, w, h, flags);
        }, py::arg("frame_id"))
        .def_static("get_frame_native_size_by_frame_id", [](uint32_t frame_id) {
            float w = 0.f, h = 0.f;
            GW::ui::GetFrameNativeSize(GW::ui::GetFrameById(frame_id), &w, &h);
            return py::make_tuple(w, h);
        }, py::arg("frame_id"))
        .def_static("get_frame_title_by_frame_id", [](uint32_t frame_id) {
            return SafeWide(GW::ui::GetFrameTitle(GW::ui::GetFrameById(frame_id)));
        }, py::arg("frame_id"))
        .def_static("get_frame_label_by_frame_id", [](uint32_t frame_id) {
            return WideToUtf8(GW::ui::GetFrameTitle(GW::ui::GetFrameById(frame_id)));
        }, py::arg("frame_id"))
        .def_static("get_frame_user_param_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::GetFrameUserParam(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("get_frame_state_bit_by_frame_id", [](uint32_t frame_id, uint32_t bit) {
            return GW::ui::GetFrameStateBit(GW::ui::GetFrameById(frame_id), bit);
        }, py::arg("frame_id"), py::arg("bit"))
        .def_static("get_frame_opacity_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::GetFrameOpacity(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))

        // ---- Frame state setters ----
        .def_static("set_frame_visible_by_frame_id", [](uint32_t frame_id, bool is_visible) {
            return GW::ui::SetFrameVisible(GW::ui::GetFrameById(frame_id), is_visible);
        }, py::arg("frame_id"), py::arg("is_visible"))
        .def_static("set_frame_disabled_by_frame_id", [](uint32_t frame_id, bool is_disabled) {
            return GW::ui::SetFrameDisabled(GW::ui::GetFrameById(frame_id), is_disabled);
        }, py::arg("frame_id"), py::arg("is_disabled"))
        .def_static("set_frame_opacity_by_frame_id", [](uint32_t frame_id, float opacity, float fade_time) {
            return GW::ui::SetFrameOpacity(GW::ui::GetFrameById(frame_id), opacity, fade_time);
        }, py::arg("frame_id"), py::arg("opacity"), py::arg("fade_time") = 0.0f)
        .def_static("show_frame_by_frame_id", [](uint32_t frame_id, bool show) {
            return GW::ui::ShowFrame(GW::ui::GetFrameById(frame_id), show);
        }, py::arg("frame_id"), py::arg("show"))
        .def_static("trigger_frame_redraw_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::TriggerFrameRedraw(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("add_frame_ui_interaction_callback_by_frame_id", [](uint32_t frame_id, uintptr_t callback_address, uintptr_t wparam) {
            return GW::ui::AddFrameUIInteractionCallback(
                GW::ui::GetFrameById(frame_id),
                reinterpret_cast<GW::ui::UIInteractionCallback>(callback_address),
                reinterpret_cast<void*>(wparam));
        }, py::arg("frame_id"), py::arg("callback_address"), py::arg("wparam") = 0)
        .def_static("destroy_ui_component_by_frame_id", [](uint32_t frame_id) {
            return GW::ui::DestroyUIComponent(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))

        // ---- Preferences ----
        .def_static("get_preference_options", [](uint32_t pref) {
            uint32_t* options = nullptr;
            const uint32_t count = GW::ui::GetPreferenceOptions(static_cast<GW::Constants::EnumPreference>(pref), &options);
            std::vector<uint32_t> out;
            if (options) {
                out.assign(options, options + count);
            }
            return out;
        }, py::arg("pref"))
        .def_static("get_enum_preference", [](uint32_t pref) {
            return GW::ui::GetPreference(static_cast<GW::Constants::EnumPreference>(pref));
        }, py::arg("pref"))
        .def_static("get_int_preference", [](uint32_t pref) {
            return GW::ui::GetPreference(static_cast<GW::Constants::NumberPreference>(pref));
        }, py::arg("pref"))
        .def_static("get_bool_preference", [](uint32_t pref) {
            return GW::ui::GetPreference(static_cast<GW::Constants::FlagPreference>(pref));
        }, py::arg("pref"))
        .def_static("get_string_preference", [](uint32_t pref) {
            return SafeWide(GW::ui::GetPreference(static_cast<GW::Constants::StringPreference>(pref)));
        }, py::arg("pref"))
        .def_static("set_enum_preference", [](uint32_t pref, uint32_t value) {
            return GW::ui::SetPreference(static_cast<GW::Constants::EnumPreference>(pref), value);
        }, py::arg("pref"), py::arg("value"))
        .def_static("set_int_preference", [](uint32_t pref, uint32_t value) {
            return GW::ui::SetPreference(static_cast<GW::Constants::NumberPreference>(pref), value);
        }, py::arg("pref"), py::arg("value"))
        .def_static("set_bool_preference", [](uint32_t pref, bool value) {
            return GW::ui::SetPreference(static_cast<GW::Constants::FlagPreference>(pref), value);
        }, py::arg("pref"), py::arg("value"))
        .def_static("set_string_preference", [](uint32_t pref, std::wstring value) {
            return GW::ui::SetPreference(static_cast<GW::Constants::StringPreference>(pref), value.data());
        }, py::arg("pref"), py::arg("value"))

        // ---- UI messages / input ----
        .def_static("SendUIMessage", &SendUIMessagePacked,
            py::arg("msgid"), py::arg("values"), py::arg("skip_hooks") = false)
        .def_static("SendUIMessageRaw", [](uint32_t msgid, uintptr_t wparam, uintptr_t lparam, bool skip_hooks) {
            return GW::ui::SendUIMessage(static_cast<UIMessage>(msgid),
                reinterpret_cast<void*>(wparam), reinterpret_cast<void*>(lparam), skip_hooks);
        }, py::arg("msgid"), py::arg("wparam"), py::arg("lparam") = 0, py::arg("skip_hooks") = false)
        .def_static("SendFrameUIMessage", [](uint32_t frame_id, uint32_t message_id, uintptr_t wparam, uintptr_t lparam) {
            return GW::ui::SendFrameUIMessage(GW::ui::GetFrameById(frame_id), static_cast<UIMessage>(message_id),
                reinterpret_cast<void*>(wparam), reinterpret_cast<void*>(lparam));
        }, py::arg("frame_id"), py::arg("message_id"), py::arg("wparam"), py::arg("lparam") = 0)
        .def_static("SendFrameUIMessageWString", [](uint32_t frame_id, uint32_t message_id, std::wstring text) {
            return GW::ui::SendFrameUIMessage(GW::ui::GetFrameById(frame_id), static_cast<UIMessage>(message_id),
                static_cast<void*>(text.data()), nullptr);
        }, py::arg("frame_id"), py::arg("message_id"), py::arg("text"))
        .def_static("button_click", [](uint32_t frame_id) {
            return GW::ui::ButtonClick(GW::ui::GetFrameById(frame_id));
        }, py::arg("frame_id"))
        .def_static("button_double_click", [](uint32_t frame_id) {
            auto* button = FrameAs<GW::ui::ButtonFrame>(frame_id);
            return button && button->DoubleClick();
        }, py::arg("frame_id"))
        .def_static("test_mouse_action", [](uint32_t frame_id, uint32_t current_state, uint32_t wparam, uint32_t lparam) {
            return GW::ui::TestMouseAction(frame_id, current_state, wparam, lparam);
        }, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam") = 0, py::arg("lparam") = 0)
        .def_static("test_mouse_click_action", [](uint32_t frame_id, uint32_t current_state, uint32_t wparam, uint32_t lparam) {
            return GW::ui::TestMouseClickAction(frame_id, current_state, wparam, lparam);
        }, py::arg("frame_id"), py::arg("current_state"), py::arg("wparam") = 0, py::arg("lparam") = 0)
        .def_static("key_down", [](uint32_t key, uint32_t frame_id) {
            return GW::ui::Keydown(static_cast<GW::ui::ControlAction>(key), GW::ui::GetFrameById(frame_id));
        }, py::arg("key"), py::arg("frame_id") = 0)
        .def_static("key_up", [](uint32_t key, uint32_t frame_id) {
            return GW::ui::Keyup(static_cast<GW::ui::ControlAction>(key), GW::ui::GetFrameById(frame_id));
        }, py::arg("key"), py::arg("frame_id") = 0)
        .def_static("key_press", [](uint32_t key, uint32_t frame_id) {
            return GW::ui::Keypress(static_cast<GW::ui::ControlAction>(key), GW::ui::GetFrameById(frame_id));
        }, py::arg("key"), py::arg("frame_id") = 0)

        // ---- Enc-string helpers ----
        .def_static("is_valid_enc_str", [](std::wstring enc) { return GW::ui::IsValidEncStr(enc.c_str()); }, py::arg("enc_str"))
        .def_static("uint32_to_enc_str", [](uint32_t value) {
            wchar_t buffer[8] = {};
            GW::ui::UInt32ToEncStr(value, buffer, 8);
            return std::wstring(buffer);
        }, py::arg("value"))
        .def_static("enc_str_to_uint32", [](std::wstring enc) { return GW::ui::EncStrToUInt32(enc.c_str()); }, py::arg("enc_str"))

        // ---- Windows ----
        .def_static("set_window_visible", [](uint32_t window_id, bool is_visible) {
            return GW::ui::SetWindowVisible(static_cast<GW::ui::WindowID>(window_id), is_visible);
        }, py::arg("window_id"), py::arg("is_visible"))

        // ---- Widget creation (native component factories) ----
        .def_static("create_ui_component_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, uintptr_t event_callback, std::wstring name_enc, std::wstring component_label) {
            return GW::ui::CreateUIComponent(parent_frame_id, component_flags, child_index,
                reinterpret_cast<GW::ui::UIInteractionCallback>(event_callback),
                name_enc.empty() ? nullptr : name_enc.data(),
                component_label.empty() ? nullptr : component_label.data());
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index"), py::arg("event_callback"),
            py::arg("name_enc") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_ui_component_raw_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, uintptr_t event_callback, uintptr_t wparam, std::wstring component_label) {
            return GW::ui::CreateUIComponent(parent_frame_id, component_flags, child_index,
                reinterpret_cast<GW::ui::UIInteractionCallback>(event_callback),
                reinterpret_cast<void*>(wparam),
                component_label.empty() ? nullptr : component_label.c_str());
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index"), py::arg("event_callback"),
            py::arg("wparam") = static_cast<uintptr_t>(0), py::arg("component_label") = std::wstring())
        .def_static("create_button_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring name_enc, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateButtonFrame(parent_frame_id, component_flags, child_index,
                name_enc.empty() ? nullptr : name_enc.data(),
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("name_enc") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_ctl_button_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring name_enc, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateCtlButtonFrame(parent_frame_id, component_flags, child_index,
                name_enc.empty() ? nullptr : name_enc.data(),
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("name_enc") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_text_button_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring caption, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateTextButtonFrame(parent_frame_id, component_flags, child_index,
                caption.empty() ? nullptr : caption.data(),
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("caption") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_flat_button_with_click_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring label_text, bool enable_click) -> uint32_t {
            Frame* frame = GW::ui::CreateFlatButtonWithClick(parent_frame_id, component_flags, child_index,
                label_text.empty() ? nullptr : label_text.data(), enable_click);
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("label_text") = std::wstring(), py::arg("enable_click") = false)
        .def_static("create_checkbox_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring name_enc, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateCheckboxFrame(parent_frame_id, component_flags, child_index,
                name_enc.empty() ? nullptr : name_enc.data(),
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("name_enc") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_scrollable_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateScrollableFrame(parent_frame_id, component_flags, child_index, nullptr,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())
        .def_static("create_text_label_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring name_enc, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateTextLabelFrame(parent_frame_id, component_flags, child_index,
                name_enc.empty() ? nullptr : name_enc.data(),
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("name_enc") = std::wstring(), py::arg("component_label") = std::wstring())
        .def_static("create_dropdown_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateDropdownFrame(parent_frame_id, component_flags, child_index,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())
        .def_static("create_slider_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateSliderFrame(parent_frame_id, component_flags, child_index,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())
        .def_static("create_editable_text_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateEditableTextFrame(parent_frame_id, component_flags, child_index,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())
        .def_static("create_progress_bar_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateProgressBar(parent_frame_id, component_flags, child_index,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())
        .def_static("create_tabs_frame_by_frame_id", [](uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index, std::wstring component_label) -> uint32_t {
            Frame* frame = GW::ui::CreateTabsFrame(parent_frame_id, component_flags, child_index,
                component_label.empty() ? nullptr : component_label.data());
            return frame ? frame->frame_id : 0;
        }, py::arg("parent_frame_id"), py::arg("component_flags"), py::arg("child_index") = 0,
            py::arg("component_label") = std::wstring())

        // ---- Button ----
        .def_static("get_button_label_by_frame_id", [](uint32_t frame_id) {
            auto* button = FrameAs<GW::ui::ButtonFrame>(frame_id);
            const wchar_t* enc = nullptr;
            if (button && button->GetLabel(&enc)) {
                return SafeWide(enc);
            }
            return std::wstring();
        }, py::arg("frame_id"))
        .def_static("set_button_label_by_frame_id", [](uint32_t frame_id, std::wstring enc_label) {
            auto* button = FrameAs<GW::ui::ButtonFrame>(frame_id);
            return button && button->SetLabel(enc_label.c_str());
        }, py::arg("frame_id"), py::arg("enc_label"))
        .def_static("button_mouse_action_by_frame_id", [](uint32_t frame_id, uint32_t action_state) {
            auto* button = FrameAs<GW::ui::ButtonFrame>(frame_id);
            return button && button->MouseAction(static_cast<GW::ui::packet::ActionState>(action_state));
        }, py::arg("frame_id"), py::arg("action_state"))

        // ---- Checkbox ----
        .def_static("is_checkbox_checked_by_frame_id", [](uint32_t frame_id) {
            auto* checkbox = FrameAs<GW::ui::CheckboxFrame>(frame_id);
            return checkbox && checkbox->IsChecked();
        }, py::arg("frame_id"))
        .def_static("set_checkbox_checked_by_frame_id", [](uint32_t frame_id, bool checked) {
            auto* checkbox = FrameAs<GW::ui::CheckboxFrame>(frame_id);
            return checkbox && checkbox->SetChecked(checked);
        }, py::arg("frame_id"), py::arg("checked"))
        .def_static("get_checkbox_value_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* checkbox = FrameAs<GW::ui::CheckboxFrame>(frame_id);
            return checkbox ? checkbox->GetValue() : 0;
        }, py::arg("frame_id"))
        .def_static("set_checkbox_value_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* checkbox = FrameAs<GW::ui::CheckboxFrame>(frame_id);
            return checkbox && checkbox->SetValue(value);
        }, py::arg("frame_id"), py::arg("value"))

        // ---- Dropdown ----
        .def_static("get_dropdown_options_by_frame_id", [](uint32_t frame_id) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown ? dropdown->GetOptions() : std::vector<uint32_t>();
        }, py::arg("frame_id"))
        .def_static("select_dropdown_option_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown && dropdown->SelectOption(value);
        }, py::arg("frame_id"), py::arg("value"))
        .def_static("select_dropdown_index_by_frame_id", [](uint32_t frame_id, uint32_t index) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown && dropdown->SelectIndex(index);
        }, py::arg("frame_id"), py::arg("index"))
        .def_static("add_dropdown_option_by_frame_id", [](uint32_t frame_id, std::wstring label_enc, uint32_t value) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown && dropdown->AddOption(label_enc.c_str(), value);
        }, py::arg("frame_id"), py::arg("label_enc"), py::arg("value"))
        .def_static("get_dropdown_count_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            uint32_t count = 0;
            if (dropdown) dropdown->GetCount(&count);
            return count;
        }, py::arg("frame_id"))
        .def_static("get_dropdown_option_value_by_frame_id", [](uint32_t frame_id, uint32_t index) -> uint32_t {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            uint32_t value = 0;
            if (dropdown) dropdown->GetOptionValue(index, &value);
            return value;
        }, py::arg("frame_id"), py::arg("index"))
        .def_static("get_dropdown_option_index_by_frame_id", [](uint32_t frame_id, uint32_t value) -> uint32_t {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            uint32_t index = 0;
            if (dropdown) dropdown->GetOptionIndex(value, &index);
            return index;
        }, py::arg("frame_id"), py::arg("value"))
        .def_static("get_dropdown_selected_index_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            uint32_t index = 0;
            if (dropdown) dropdown->GetSelectedIndex(&index);
            return index;
        }, py::arg("frame_id"))
        .def_static("dropdown_has_value_mapping_by_frame_id", [](uint32_t frame_id) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown && dropdown->HasValueMapping();
        }, py::arg("frame_id"))
        .def_static("get_dropdown_value_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown ? dropdown->GetValue() : 0;
        }, py::arg("frame_id"))
        .def_static("set_dropdown_value_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* dropdown = FrameAs<GW::ui::DropdownFrame>(frame_id);
            return dropdown && dropdown->SetValue(value);
        }, py::arg("frame_id"), py::arg("value"))

        // ---- Slider ----
        .def_static("get_slider_value_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* slider = FrameAs<GW::ui::SliderFrame>(frame_id);
            uint32_t value = 0;
            if (slider) slider->GetValue(&value);
            return value;
        }, py::arg("frame_id"))
        .def_static("set_slider_value_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* slider = FrameAs<GW::ui::SliderFrame>(frame_id);
            return slider && slider->SetValue(value);
        }, py::arg("frame_id"), py::arg("value"))

        // ---- Editable text ----
        .def_static("get_editable_text_value_by_frame_id", [](uint32_t frame_id) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit ? SafeWide(edit->GetValue()) : std::wstring();
        }, py::arg("frame_id"))
        .def_static("set_editable_text_value_by_frame_id", [](uint32_t frame_id, std::wstring value) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->SetValue(value.c_str());
        }, py::arg("frame_id"), py::arg("value"))
        .def_static("set_editable_text_max_length_by_frame_id", [](uint32_t frame_id, uint32_t max_length) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->SetMaxLength(max_length);
        }, py::arg("frame_id"), py::arg("max_length"))
        .def_static("is_editable_text_read_only_by_frame_id", [](uint32_t frame_id) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->IsReadOnly();
        }, py::arg("frame_id"))
        .def_static("set_editable_text_read_only_by_frame_id", [](uint32_t frame_id, bool read_only) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->SetReadOnly(read_only);
        }, py::arg("frame_id"), py::arg("read_only"))
        .def_static("set_read_only_by_frame_id", [](uint32_t frame_id, bool read_only) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->SetReadOnly(read_only);
        }, py::arg("frame_id"), py::arg("is_read_only"))
        .def_static("is_read_only_by_frame_id", [](uint32_t frame_id) {
            auto* edit = FrameAs<GW::ui::EditableTextFrame>(frame_id);
            return edit && edit->IsReadOnly();
        }, py::arg("frame_id"))

        // ---- Progress bar ----
        .def_static("get_progress_bar_value_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* bar = FrameAs<GW::ui::ProgressBar>(frame_id);
            return bar ? bar->GetValue() : 0;
        }, py::arg("frame_id"))
        .def_static("set_progress_bar_value_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* bar = FrameAs<GW::ui::ProgressBar>(frame_id);
            return bar && bar->SetValue(value);
        }, py::arg("frame_id"), py::arg("value"))
        .def_static("set_progress_bar_max_by_frame_id", [](uint32_t frame_id, uint32_t value) {
            auto* bar = FrameAs<GW::ui::ProgressBar>(frame_id);
            return bar && bar->SetMax(value);
        }, py::arg("frame_id"), py::arg("value"))
        .def_static("set_progress_bar_color_id_by_frame_id", [](uint32_t frame_id, uint32_t color_id) {
            auto* bar = FrameAs<GW::ui::ProgressBar>(frame_id);
            return bar && bar->SetColorId(color_id);
        }, py::arg("frame_id"), py::arg("color_id"))
        .def_static("set_progress_bar_style_by_frame_id", [](uint32_t frame_id, uint32_t style) {
            auto* bar = FrameAs<GW::ui::ProgressBar>(frame_id);
            return bar && bar->SetStyle(static_cast<GW::Constants::ProgressBarStyle>(style));
        }, py::arg("frame_id"), py::arg("style"))

        // ---- Text labels ----
        .def_static("get_text_label_encoded_by_frame_id", [](uint32_t frame_id) {
            auto* label = FrameAs<GW::ui::TextLabelFrame>(frame_id);
            return label ? SafeWide(label->GetEncodedLabel()) : std::wstring();
        }, py::arg("frame_id"))
        .def_static("get_text_label_decoded_by_frame_id", [](uint32_t frame_id) {
            auto* label = FrameAs<GW::ui::TextLabelFrame>(frame_id);
            return label ? SafeWide(label->GetDecodedLabel()) : std::wstring();
        }, py::arg("frame_id"))
        .def_static("set_text_label_by_frame_id", [](uint32_t frame_id, std::wstring enc_label) {
            auto* label = FrameAs<GW::ui::TextLabelFrame>(frame_id);
            return label && label->SetLabel(enc_label.c_str());
        }, py::arg("frame_id"), py::arg("label"))
        .def_static("set_label_by_frame_id", [](uint32_t frame_id, std::wstring enc_label) {
            auto* button = FrameAs<GW::ui::ButtonFrame>(frame_id);
            return button && button->SetLabel(enc_label.c_str());
        }, py::arg("frame_id"), py::arg("label"))
        .def_static("set_multiline_label_by_frame_id", [](uint32_t frame_id, std::wstring enc_label) {
            auto* label = FrameAs<GW::ui::MultiLineTextLabelFrame>(frame_id);
            return label && label->SetLabel(enc_label.c_str());
        }, py::arg("frame_id"), py::arg("label"))
        .def_static("set_text_label_font_by_frame_id", [](uint32_t frame_id, uint32_t font_id) {
            auto* label = FrameAs<GW::ui::TextLabelFrame>(frame_id);
            return label && label->SetFont(font_id);
        }, py::arg("frame_id"), py::arg("font_id"))

        // ---- Tabs ----
        .def_static("add_tab_by_frame_id", [](uint32_t frame_id, std::wstring tab_name_enc, uint32_t flags, uint32_t child_offset_id, uintptr_t callback_address, uintptr_t wparam) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            if (!tabs) return 0;
            Frame* tab = tabs->AddTab(tab_name_enc.c_str(), flags, child_offset_id,
                reinterpret_cast<GW::ui::UIInteractionCallback>(callback_address),
                reinterpret_cast<void*>(wparam));
            return tab ? tab->frame_id : 0;
        }, py::arg("frame_id"), py::arg("tab_name_enc"), py::arg("flags"), py::arg("child_offset_id"),
            py::arg("callback_address") = 0, py::arg("wparam") = 0)
        .def_static("disable_tab_by_frame_id", [](uint32_t frame_id, uint32_t tab_id) {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            return tabs && tabs->DisableTab(tab_id);
        }, py::arg("frame_id"), py::arg("tab_id"))
        .def_static("enable_tab_by_frame_id", [](uint32_t frame_id, uint32_t tab_id) {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            return tabs && tabs->EnableTab(tab_id);
        }, py::arg("frame_id"), py::arg("tab_id"))
        .def_static("remove_tab_by_frame_id", [](uint32_t frame_id, uint32_t tab_id) {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            return tabs && tabs->RemoveTab(tab_id);
        }, py::arg("frame_id"), py::arg("tab_id"))
        .def_static("get_current_tab_index_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            uint32_t tab_id = 0;
            if (tabs) tabs->GetCurrentTabIndex(&tab_id);
            return tab_id;
        }, py::arg("frame_id"))
        .def_static("get_tab_frame_id_by_frame_id", [](uint32_t frame_id, uint32_t tab_id) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            uint32_t tab_frame_id = 0;
            if (tabs) tabs->GetTabFrameId(tab_id, &tab_frame_id);
            return tab_frame_id;
        }, py::arg("frame_id"), py::arg("tab_id"))
        .def_static("get_tab_frame_id", [](uint32_t parent_frame_id, uint32_t index) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(parent_frame_id);
            uint32_t tab_frame_id = 0;
            if (tabs) tabs->GetTabFrameId(index, &tab_frame_id);
            return tab_frame_id;
        }, py::arg("parent_frame_id"), py::arg("index"))
        .def_static("get_is_tab_enabled_by_frame_id", [](uint32_t frame_id, uint32_t tab_id) -> bool {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            uint32_t is_enabled = 0;
            if (tabs) tabs->GetIsTabEnabled(tab_id, &is_enabled);
            return is_enabled != 0;
        }, py::arg("frame_id"), py::arg("tab_id"))
        .def_static("get_tab_by_label_by_frame_id", [](uint32_t frame_id, std::wstring label) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            Frame* tab = tabs ? tabs->GetTabByLabel(label.c_str()) : nullptr;
            return tab ? tab->frame_id : 0;
        }, py::arg("frame_id"), py::arg("label"))
        .def_static("get_current_tab_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            Frame* tab = tabs ? tabs->GetCurrentTab() : nullptr;
            return tab ? tab->frame_id : 0;
        }, py::arg("frame_id"))
        .def_static("choose_tab_by_tab_frame_id", [](uint32_t frame_id, uint32_t tab_frame_id) {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            return tabs && tabs->ChooseTab(GW::ui::GetFrameById(tab_frame_id));
        }, py::arg("frame_id"), py::arg("tab_frame_id"))
        .def_static("choose_tab_by_index_by_frame_id", [](uint32_t frame_id, uint32_t tab_index) {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            return tabs && tabs->ChooseTab(tab_index);
        }, py::arg("frame_id"), py::arg("tab_index"))
        .def_static("get_tab_button_by_frame_id", [](uint32_t frame_id, uint32_t tab_frame_id) -> uint32_t {
            auto* tabs = FrameAs<GW::ui::TabsFrame>(frame_id);
            GW::ui::ButtonFrame* button = tabs ? tabs->GetTabButton(GW::ui::GetFrameById(tab_frame_id)) : nullptr;
            return button ? button->frame_id : 0;
        }, py::arg("frame_id"), py::arg("tab_frame_id"))

        // ---- Scrollable ----
        .def_static("clear_scrollable_items_by_frame_id", [](uint32_t frame_id) {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable && scrollable->ClearItems();
        }, py::arg("frame_id"))
        .def_static("remove_scrollable_item_by_frame_id", [](uint32_t frame_id, uint32_t child_offset_id) {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable && scrollable->RemoveItem(child_offset_id);
        }, py::arg("frame_id"), py::arg("child_offset_id"))
        .def_static("add_scrollable_item_by_frame_id", [](uint32_t frame_id, uint32_t flags, uint32_t child_offset_id, uintptr_t callback_address) {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable && scrollable->AddItem(flags, child_offset_id,
                reinterpret_cast<GW::ui::UIInteractionCallback>(callback_address));
        }, py::arg("frame_id"), py::arg("flags"), py::arg("child_offset_id"), py::arg("callback_address") = 0)
        .def_static("get_scrollable_item_frame_id_by_frame_id", [](uint32_t frame_id, uint32_t child_offset_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable ? scrollable->GetItemFrameId(child_offset_id) : 0;
        }, py::arg("frame_id"), py::arg("child_offset_id"))
        .def_static("get_scrollable_selected_value_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            uint32_t value = 0;
            if (scrollable) scrollable->GetSelectedValue(&value);
            return value;
        }, py::arg("frame_id"))
        .def_static("get_scrollable_first_child_frame_id_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable ? scrollable->GetFirstChildFrameId() : 0;
        }, py::arg("frame_id"))
        .def_static("get_scrollable_next_child_frame_id_by_frame_id", [](uint32_t frame_id, uint32_t child_frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable ? scrollable->GetNextChildFrameId(child_frame_id) : 0;
        }, py::arg("frame_id"), py::arg("child_frame_id"))
        .def_static("get_scrollable_last_child_frame_id_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable ? scrollable->GetLastChildFrameId() : 0;
        }, py::arg("frame_id"))
        .def_static("get_scrollable_prev_child_frame_id_by_frame_id", [](uint32_t frame_id, uint32_t child_frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            return scrollable ? scrollable->GetPrevChildFrameId(child_frame_id) : 0;
        }, py::arg("frame_id"), py::arg("child_frame_id"))
        .def_static("get_scrollable_item_rect_by_frame_id", [](uint32_t frame_id, uint32_t child_offset_id) {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            float rect[4] = {};
            if (scrollable) scrollable->GetItemRect(child_offset_id, rect);
            return py::make_tuple(rect[0], rect[1], rect[2], rect[3]);
        }, py::arg("frame_id"), py::arg("child_offset_id"))
        .def_static("get_scrollable_count_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            uint32_t count = 0;
            if (scrollable) scrollable->GetCount(&count);
            return count;
        }, py::arg("frame_id"))
        .def_static("get_scrollable_items_by_frame_id", [](uint32_t frame_id) {
            std::vector<uint32_t> out;
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            if (!scrollable) return out;
            const uint32_t count = scrollable->GetItems();
            if (!count) return out;
            out.resize(count);
            scrollable->GetItems(out.data(), count);
            return out;
        }, py::arg("frame_id"))
        .def_static("get_scrollable_page_by_frame_id", [](uint32_t frame_id) -> uint32_t {
            auto* scrollable = FrameAs<GW::ui::ScrollableFrame>(frame_id);
            Frame* page = scrollable ? scrollable->GetPage() : nullptr;
            return page ? page->frame_id : 0;
        }, py::arg("frame_id"));
}
