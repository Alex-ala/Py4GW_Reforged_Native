#include "base/error_handling.h"

#include "GW/agent_recolor/agent_recolor.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"

namespace GW::agent_recolor {

bool ResolveFindCharFunction() {
    CrashContextScope context("startup", "agent_recolor", "resolve_find_char_func");
    PY4GW::Patterns::Resolve("agent_recolor.find_char_func", &g_find_char);
    return Logger::AssertAddress(
        "ManagerFindChar_Func",
        reinterpret_cast<uintptr_t>(g_find_char),
        "agent_recolor");
}

bool ResolveConsiderColorResolver() {
    CrashContextScope context("startup", "agent_recolor", "resolve_consider_color_resolver");
    PY4GW::Patterns::Resolve("agent_recolor.consider_color_resolver_func", &g_resolver);
    return Logger::AssertAddress(
        "GetConsiderColor_Resolver_Func",
        reinterpret_cast<uintptr_t>(g_resolver),
        "agent_recolor");
}

}  // namespace GW::agent_recolor
