#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/agent_recolor/agent_recolor.h"

namespace py = pybind11;

using GW::agent_recolor::AgentRecolor;

PYBIND11_EMBEDDED_MODULE(PyAgentRecolor, m) {
    m.doc() = "Agent name-tag color override. DLL init owns the resolver detour; "
              "Python controls enable/disable and the color rule store. Colors are ARGB 0xAARRGGBB.";

    m.def("enable", []() { AgentRecolor::Instance().Enable(); },
        "Enable color overriding (the resolver detour applies matching rules).");
    m.def("disable", []() { AgentRecolor::Instance().Disable(); },
        "Disable color overriding (game default colors return).");
    m.def("is_enabled", []() { return AgentRecolor::Instance().IsEnabled(); });
    m.def("is_hook_installed", []() { return AgentRecolor::Instance().IsHookInstalled(); });

    m.def("set_agent_color",
        [](uint32_t agent_id, uint32_t argb) { AgentRecolor::Instance().SetAgentColor(agent_id, argb); },
        py::arg("agent_id"), py::arg("argb"),
        "Override one agent's name-tag color (ARGB 0xAARRGGBB). Highest precedence.");
    m.def("remove_agent_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().RemoveAgentColor(agent_id); },
        py::arg("agent_id"));

    m.def("set_allegiance_color",
        [](int allegiance, uint32_t argb) { AgentRecolor::Instance().SetAllegianceColor(allegiance, argb); },
        py::arg("allegiance"), py::arg("argb"),
        "Override a whole allegiance category (1=Ally..6=NpcMinipet). Per-agent rules win.");
    m.def("remove_allegiance_color",
        [](int allegiance) { return AgentRecolor::Instance().RemoveAllegianceColor(allegiance); },
        py::arg("allegiance"));

    m.def("clear_rules", []() { AgentRecolor::Instance().ClearRules(); },
        "Drop all color overrides (agents revert to game defaults).");

    m.def("get_agent_rules", []() { return AgentRecolor::Instance().GetAgentRules(); });
    m.def("get_allegiance_rules", []() { return AgentRecolor::Instance().GetAllegianceRules(); });

    m.def("read_consider_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().ReadConsiderColor(agent_id); },
        py::arg("agent_id"),
        "Read the color the game currently computes for an agent (ARGB). Read-only; "
        "uses the original resolver, so it is unaffected by overrides.");

    m.def("get_diagnostics", []() {
        const auto diag = AgentRecolor::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = diag.initialized;
        out["hook_installed"] = diag.hook_installed;
        out["enabled"] = diag.enabled;
        out["resolver_calls_seen"] = diag.resolver_calls_seen;
        out["agent_rule_hits"] = diag.agent_rule_hits;
        out["allegiance_rule_hits"] = diag.allegiance_rule_hits;
        out["last_agent_id"] = diag.last_agent_id;
        out["last_color"] = diag.last_color;
        return out;
    });
    m.def("reset_diagnostics", []() { AgentRecolor::Instance().ResetDiagnostics(); });
}
