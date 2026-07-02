#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "system/system.h"

#include <utility>

namespace py = pybind11;

namespace {

bool MapReady() {
    return GW::map::GetIsMapLoaded() &&
        GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyGameThread, m) {
    m.doc() = "Py4GW GameThread bindings";

    m.def("clear_calls", []() {
        GW::game_thread::ClearCalls();
    });

    m.def("is_in_game_thread", []() -> bool {
        return GW::game_thread::IsInGameThread();
    });

    // Enqueue a Python callable to run on the GW game thread. Parity port of the
    // legacy EnqueuePythonCallback: guarded by a map-ready check both here and
    // again on the game thread (the map can change in between). Deviation from
    // legacy: a failing callback is reported to the console instead of being
    // silently swallowed.
    m.def("enqueue", [](py::function fn) {
        if (!MapReady()) {
            return;
        }
        GW::game_thread::Enqueue([fn = std::move(fn)]() mutable {
            if (!MapReady()) {
                return;
            }
            py::gil_scoped_acquire gil;
            try {
                fn();
            } catch (const py::error_already_set& error) {
                PY4GW::System::Instance().WriteConsoleMessage(
                    "PyGameThread", MessageType::Error, std::string("Game thread callback error: ") + error.what());
            }
        });
    }, py::arg("fn"), "Enqueue a Python callable to run on the GW game thread");
}
