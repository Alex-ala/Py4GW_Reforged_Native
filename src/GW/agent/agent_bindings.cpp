#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/agent/agent.h"

#include <string>

namespace py = pybind11;

namespace {

std::string WideToStr(const wchar_t* wstr) {
    if (!wstr) return {};
    std::string result;
    while (*wstr) {
        wchar_t ch = *wstr++;
        result.push_back(static_cast<char>(ch < 128 ? ch : '?'));
    }
    return result;
}

} // namespace

PYBIND11_EMBEDDED_MODULE(PyAgent, m) {
    m.doc() = "Py4GW Agent bindings";

    m.def("send_dialog", [](uint32_t dialog_id) {
        return GW::agent::SendDialog(dialog_id);
    }, py::arg("dialog_id"));

    m.def("get_observing_id", []() -> uint32_t {
        return GW::agent::GetObservingId();
    });

    m.def("get_controlled_character_id", []() -> uint32_t {
        return GW::agent::GetControlledCharacterId();
    });

    m.def("get_target_id", []() -> uint32_t {
        return GW::agent::GetTargetId();
    });

    m.def("get_amount_of_players_in_instance", []() -> uint32_t {
        return GW::agent::GetAmountOfPlayersInInstance();
    });

    m.def("is_observing", []() -> bool {
        return GW::agent::IsObserving();
    });

    m.def("change_target", [](uint32_t agent_id) {
        return GW::agent::ChangeTarget(static_cast<GW::agent::AgentID>(agent_id));
    }, py::arg("agent_id"));

    m.def("move", [](float x, float y, uint32_t zplane) {
        return GW::agent::Move(x, y, zplane);
    }, py::arg("x"), py::arg("y"), py::arg("zplane") = 0);

    m.def("interact_agent", [](uint32_t agent_id, bool call_target) {
        auto* agent = GW::agent::GetAgentByID(agent_id);
        if (!agent) return false;
        return GW::agent::InteractAgent(agent, call_target);
    }, py::arg("agent_id"), py::arg("call_target") = false);

    m.def("call_target", [](uint32_t agent_id) {
        return GW::agent::CallTarget(agent_id);
    }, py::arg("agent_id"));

    m.def("get_player_name_by_login_number", [](uint32_t login_number) -> std::string {
        return WideToStr(GW::agent::GetPlayerNameByLoginNumber(login_number));
    }, py::arg("login_number"));

    m.def("get_agent_id_by_login_number", [](uint32_t login_number) -> uint32_t {
        return GW::agent::GetAgentIdByLoginNumber(login_number);
    }, py::arg("login_number"));

    m.def("get_hero_agent_id", [](uint32_t hero_index) -> uint32_t {
        return GW::agent::GetHeroAgentID(hero_index);
    }, py::arg("hero_index"));

    m.def("get_agent_enc_name", [](uint32_t agent_id) -> std::string {
        return WideToStr(GW::agent::GetAgentEncName(agent_id));
    }, py::arg("agent_id"));

    m.def("get_agent_is_targettable", [](uint32_t agent_id) -> bool {
        auto* agent = GW::agent::GetAgentByID(agent_id);
        if (!agent) return false;
        return GW::agent::GetIsAgentTargettable(agent);
    }, py::arg("agent_id"));
}
