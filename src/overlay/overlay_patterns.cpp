#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/patterns.h"
#include "overlay/overlay.h"

namespace PY4GW::overlay {

// Definitions for the module-owned resolved symbols.
ScreenToWorldPoint_pt g_screen_to_world_point_func = nullptr;
GW::Vec2f* g_ndc_screen_coords = nullptr;

bool ResolveScreenToWorldPointFunc() {
    if (g_screen_to_world_point_func) {
        return true;
    }
    CrashContextScope context("runtime", "overlay", "resolve_screen_to_world_point");
    return PY4GW::Patterns::Resolve("overlay.screen_to_world_point_func", &g_screen_to_world_point_func);
}

bool ResolveNdcScreenCoords() {
    if (g_ndc_screen_coords) {
        return true;
    }
    CrashContextScope context("runtime", "overlay", "resolve_ndc_screen_coords");
    return PY4GW::Patterns::Resolve("overlay.ndc_screen_coords_ptr", &g_ndc_screen_coords);
}

}  // namespace PY4GW::overlay
