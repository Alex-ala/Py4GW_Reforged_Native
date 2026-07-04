#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/patterns.h"
#include "GW/pathing/pathing.h"

namespace GW::pathing {

// Definition for the module-owned resolved symbol.
FindPath_pt g_find_path_func = nullptr;

bool ResolveFindPathFunc() {
    if (g_find_path_func) {
        return true;
    }
    CrashContextScope context("runtime", "pathing", "resolve_find_path");
    return PY4GW::Patterns::Resolve("pathing.find_path_func", &g_find_path_func);
}

}  // namespace GW::pathing
