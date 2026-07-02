#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/effects/effects.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyEffects, m) {
    m.doc() = "Py4GW Effects bindings";

    m.def("get_alcohol_level", []() -> uint32_t {
        return GW::effects::GetAlcoholLevel();
    });

    m.def("get_drunk_af", [](uint32_t intensity, uint32_t tint) {
        GW::effects::GetDrunkAf(intensity, tint);
    }, py::arg("intensity"), py::arg("tint"));

    m.def("drop_buff", [](uint32_t buff_id) {
        return GW::effects::DropBuff(buff_id);
    }, py::arg("buff_id"));

    m.def("effect_count", [](uint32_t agent_id) -> uint32_t {
        auto* effects = GW::effects::GetAgentEffects(agent_id);
        return (effects && effects->valid()) ? effects->size() : 0;
    }, py::arg("agent_id"));

    m.def("buff_count", [](uint32_t agent_id) -> uint32_t {
        auto* buffs = GW::effects::GetAgentBuffs(agent_id);
        return (buffs && buffs->valid()) ? buffs->size() : 0;
    }, py::arg("agent_id"));

    m.def("effect_exists", [](uint32_t agent_id, uint32_t skill_id) -> bool {
        auto* effects = GW::effects::GetAgentEffects(agent_id);
        const auto requested_skill_id = static_cast<GW::Constants::SkillID>(skill_id);
        if (!effects || !effects->valid()) return false;
        for (size_t i = 0; i < effects->size(); ++i) {
            if ((*effects)[i].skill_id == requested_skill_id) return true;
        }
        return false;
    }, py::arg("agent_id"), py::arg("skill_id"));

    m.def("buff_exists", [](uint32_t agent_id, uint32_t skill_id) -> bool {
        auto* buffs = GW::effects::GetAgentBuffs(agent_id);
        const auto requested_skill_id = static_cast<GW::Constants::SkillID>(skill_id);
        if (!buffs || !buffs->valid()) return false;
        for (size_t i = 0; i < buffs->size(); ++i) {
            if ((*buffs)[i].skill_id == requested_skill_id) return true;
        }
        return false;
    }, py::arg("agent_id"), py::arg("skill_id"));
}
