#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/common/game_pos.h"
#include "GW/party/party.h"

#include <string>

namespace py = pybind11;

namespace {

std::wstring StrToWide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

} // namespace

PYBIND11_EMBEDDED_MODULE(PyParty, m) {
    m.doc() = "Py4GW Party bindings";

    m.def("set_tick_toggle", [](bool enable) {
        GW::party::set_tick_toggle(enable);
    }, py::arg("enable"));

    m.def("tick", [](bool flag) -> bool {
        return GW::party::tick(flag);
    }, py::arg("flag") = true);

    m.def("get_party_size", []() -> uint32_t { return GW::party::get_party_size(); });

    m.def("get_party_player_count", []() -> uint32_t { return GW::party::get_party_player_count(); });

    m.def("get_party_hero_count", []() -> uint32_t { return GW::party::get_party_hero_count(); });

    m.def("get_party_henchman_count", []() -> uint32_t { return GW::party::get_party_henchman_count(); });

    m.def("get_is_party_defeated", []() -> bool { return GW::party::get_is_party_defeated(); });

    m.def("get_is_party_in_hard_mode", []() -> bool { return GW::party::get_is_party_in_hard_mode(); });

    m.def("get_is_hard_mode_unlocked", []() -> bool { return GW::party::get_is_hard_mode_unlocked(); });

    m.def("get_is_party_ticked", []() -> bool { return GW::party::get_is_party_ticked(); });

    m.def("get_is_player_ticked", [](uint32_t player_index) -> bool {
        return GW::party::get_is_player_ticked(player_index);
    }, py::arg("player_index") = 0xFFFFFFFF);

    m.def("get_is_player_loaded", [](uint32_t player_index) -> bool {
        return GW::party::get_is_player_loaded(player_index);
    }, py::arg("player_index") = 0xFFFFFFFF);

    m.def("get_is_party_loaded", []() -> bool { return GW::party::get_is_party_loaded(); });

    m.def("get_is_leader", []() -> bool { return GW::party::get_is_leader(); });

    m.def("set_hard_mode", [](bool flag) -> bool { return GW::party::set_hard_mode(flag); }, py::arg("flag"));

    m.def("return_to_outpost", []() -> bool { return GW::party::return_to_outpost(); });

    m.def("respond_to_party_request", [](uint32_t party_id, bool accept) -> bool {
        return GW::party::respond_to_party_request(party_id, accept);
    }, py::arg("party_id"), py::arg("accept"));

    m.def("leave_party", []() -> bool { return GW::party::leave_party(); });

    m.def("add_hero", [](uint32_t hero_id) -> bool { return GW::party::add_hero(hero_id); }, py::arg("hero_id"));

    m.def("kick_hero", [](uint32_t hero_id) -> bool { return GW::party::kick_hero(hero_id); }, py::arg("hero_id"));

    m.def("kick_all_heroes", []() -> bool { return GW::party::kick_all_heroes(); });

    m.def("add_henchman", [](uint32_t agent_id) -> bool { return GW::party::add_henchman(agent_id); }, py::arg("agent_id"));

    m.def("kick_henchman", [](uint32_t agent_id) -> bool { return GW::party::kick_henchman(agent_id); }, py::arg("agent_id"));

    m.def("invite_player_by_id", [](uint32_t player_id) -> bool {
        return GW::party::invite_player(player_id);
    }, py::arg("player_id"));

    m.def("invite_player_by_name", [](const std::string& player_name) -> bool {
        auto wname = StrToWide(player_name);
        return GW::party::invite_player(wname.c_str());
    }, py::arg("player_name"));

    m.def("kick_player", [](uint32_t player_id) -> bool { return GW::party::kick_player(player_id); }, py::arg("player_id"));

    m.def("flag_hero", [](uint32_t hero_index, float x, float y) -> bool {
        return GW::party::flag_hero(hero_index, GW::GamePos(x, y));
    }, py::arg("hero_index"), py::arg("x"), py::arg("y"));

    m.def("flag_hero_agent", [](uint32_t agent_id, float x, float y) -> bool {
        return GW::party::flag_hero_agent(static_cast<GW::party::AgentID>(agent_id), GW::GamePos(x, y));
    }, py::arg("agent_id"), py::arg("x"), py::arg("y"));

    m.def("unflag_hero", [](uint32_t hero_index) -> bool {
        return GW::party::unflag_hero(hero_index);
    }, py::arg("hero_index"));

    m.def("flag_all", [](float x, float y) -> bool {
        return GW::party::flag_all(GW::GamePos(x, y));
    }, py::arg("x"), py::arg("y"));

    m.def("unflag_all", []() -> bool { return GW::party::unflag_all(); });

    m.def("set_hero_behavior", [](uint32_t agent_id, uint32_t behavior) -> bool {
        return GW::party::set_hero_behavior(agent_id, static_cast<GW::Constants::HeroBehavior>(behavior));
    }, py::arg("agent_id"), py::arg("behavior"));

    m.def("set_hero_skill_ai_enabled", [](uint32_t hero_agent_id, uint32_t skill_slot, bool enabled) -> bool {
        return GW::party::set_hero_skill_ai_enabled(hero_agent_id, skill_slot, enabled);
    }, py::arg("hero_agent_id"), py::arg("skill_slot"), py::arg("enabled"));

    m.def("set_pet_behavior", [](uint32_t behavior, uint32_t lock_target_id) -> bool {
        return GW::party::set_pet_behavior(static_cast<GW::Constants::HeroBehavior>(behavior), lock_target_id);
    }, py::arg("behavior"), py::arg("lock_target_id") = 0);

    m.def("get_hero_agent_id", [](uint32_t hero_index) -> uint32_t {
        return GW::party::get_hero_agent_id(hero_index);
    }, py::arg("hero_index"));

    m.def("get_agent_hero_id", [](uint32_t agent_id) -> uint32_t {
        return GW::party::get_agent_hero_id(static_cast<GW::party::AgentID>(agent_id));
    }, py::arg("agent_id"));

    m.def("search_party", [](uint32_t search_type, const std::string& advertisement) -> bool {
        auto wad = StrToWide(advertisement);
        return GW::party::search_party(search_type, advertisement.empty() ? nullptr : wad.c_str());
    }, py::arg("search_type"), py::arg("advertisement") = "");

    m.def("search_party_cancel", []() -> bool { return GW::party::search_party_cancel(); });

    m.def("search_party_reply", [](bool accept) -> bool {
        return GW::party::search_party_reply(accept);
    }, py::arg("accept"));
}
