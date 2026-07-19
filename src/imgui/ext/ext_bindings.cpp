#include "imgui/ext/ext.h"

#include <cstdint>
#include <string>

// pybind registration for PyImGui.Ext. Kept separate from the widget implementation
// (ext.cpp) so the binding glue and the ImGui logic do not mix. Binds the Ext class's
// static methods; no widget logic lives here.

namespace PY4GW::ext {

void register_ext(py::module_& parent) {
    py::module_ m = parent.def_submodule("Ext", "PyImGui extensions: composite native widgets.");

    m.def("ImageButton", &Ext::ImageButton, py::arg("label"), py::arg("texture_path"),
          py::arg("width") = 32.0f, py::arg("height") = 32.0f, py::arg("disabled") = false);

    // Launch-bar-specific composites live in their own nested submodule so the generic
    // Ext surface stays uncluttered: Ext.LaunchBar.*.
    py::module_ lb = m.def_submodule("LaunchBar", "Ext composites specific to the launch bar.");
    lb.def("IconTile", &Ext::IconTile, py::arg("label"), py::arg("x"), py::arg("y"), py::arg("width"),
           py::arg("height"), py::arg("texture_path") = std::string(), py::arg("disabled") = false,
           py::arg("tooltip") = std::string(), py::arg("overlay_fill") = 0u, py::arg("overlay_outline") = 0u);
}

}  // namespace PY4GW::ext
