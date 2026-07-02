#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>
#include <imgui_internal.h>  // DockBuilder* layout API

namespace PY4GW::imgui_bindings {

void register_docking(py::module_& m) {
    // ── docking entry points ──
    m.def("dock_space", [](ImGuiID id, const ImVec2& size, int flags) -> ImGuiID { return ImGui::DockSpace(id, size, flags); },
          py::arg("id"), py::arg("size") = ImVec2(0, 0), py::arg("flags") = 0);
    m.def("dock_space_over_viewport", [](ImGuiID id, int flags) -> ImGuiID { return ImGui::DockSpaceOverViewport(id, nullptr, flags); },
          py::arg("dockspace_id") = 0, py::arg("flags") = 0);
    m.def("set_next_window_dock_id", &ImGui::SetNextWindowDockID, py::arg("dock_id"), py::arg("cond") = 0);
    m.def("get_window_dock_id", &ImGui::GetWindowDockID);
    m.def("is_window_docked", &ImGui::IsWindowDocked);

    // ── DockBuilder: author dock layouts programmatically from Python ──
    // Typical use (once, when building a layout):
    //   root = imgui.get_id("MyDockSpace")
    //   imgui.dock_builder_remove_node(root)
    //   imgui.dock_builder_add_node(root, imgui.DockNodeFlags.PassthruCentralNode)
    //   imgui.dock_builder_set_node_size(root, imgui.get_io().display_size)
    //   left, center = imgui.dock_builder_split_node(root, imgui.Dir.Left, 0.25)
    //   imgui.dock_builder_dock_window("Tools", left)
    //   imgui.dock_builder_finish(root)
    m.def("dock_builder_dock_window", [](const char* window_name, ImGuiID node_id) { ImGui::DockBuilderDockWindow(window_name, node_id); },
          py::arg("window_name"), py::arg("node_id"));
    m.def("dock_builder_add_node", [](ImGuiID node_id, int flags) -> ImGuiID { return ImGui::DockBuilderAddNode(node_id, flags); },
          py::arg("node_id") = 0, py::arg("flags") = 0);
    m.def("dock_builder_remove_node", [](ImGuiID node_id) { ImGui::DockBuilderRemoveNode(node_id); }, py::arg("node_id"));
    m.def("dock_builder_remove_node_child_nodes", [](ImGuiID node_id) { ImGui::DockBuilderRemoveNodeChildNodes(node_id); }, py::arg("node_id"));
    m.def("dock_builder_remove_node_docked_windows", [](ImGuiID node_id, bool clear_settings_refs) { ImGui::DockBuilderRemoveNodeDockedWindows(node_id, clear_settings_refs); },
          py::arg("node_id"), py::arg("clear_settings_refs") = true);
    m.def("dock_builder_set_node_pos", [](ImGuiID node_id, const ImVec2& pos) { ImGui::DockBuilderSetNodePos(node_id, pos); }, py::arg("node_id"), py::arg("pos"));
    m.def("dock_builder_set_node_size", [](ImGuiID node_id, const ImVec2& size) { ImGui::DockBuilderSetNodeSize(node_id, size); }, py::arg("node_id"), py::arg("size"));
    m.def("dock_builder_split_node", [](ImGuiID node_id, int split_dir, float size_ratio_for_node_at_dir) {
              ImGuiID at_dir = 0, at_opposite = 0;
              ImGuiID remaining = ImGui::DockBuilderSplitNode(node_id, static_cast<ImGuiDir>(split_dir),
                                                              size_ratio_for_node_at_dir, &at_dir, &at_opposite);
              // returns (node_at_dir, node_at_opposite_dir); `remaining` is the
              // updated parent id and equals node_at_opposite_dir.
              (void)remaining;
              return py::make_tuple(at_dir, at_opposite);
          }, py::arg("node_id"), py::arg("split_dir"), py::arg("size_ratio_for_node_at_dir"),
          "Split a node; returns (node_at_dir, node_at_opposite_dir).")
    ;
    m.def("dock_builder_finish", [](ImGuiID node_id) { ImGui::DockBuilderFinish(node_id); }, py::arg("node_id"));

    // ImGuiDir for split_dir arguments.
    py::enum_<ImGuiDir>(m, "Dir")
        .value("None", ImGuiDir_None).value("Left", ImGuiDir_Left).value("Right", ImGuiDir_Right)
        .value("Up", ImGuiDir_Up).value("Down", ImGuiDir_Down)
        .export_values();
}

}  // namespace PY4GW::imgui_bindings
