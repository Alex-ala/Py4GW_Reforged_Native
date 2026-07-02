#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/player/player.h"

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

std::wstring StrToWide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

} // namespace

PYBIND11_EMBEDDED_MODULE(PyPlayer, m) {
    m.doc() = "Py4GW Player bindings";

    m.def("set_active_title", [](uint32_t title_id) -> bool {
        return GW::player::SetActiveTitle(static_cast<GW::Constants::TitleID>(title_id));
    }, py::arg("title_id"));

    m.def("remove_active_title", []() -> bool {
        return GW::player::RemoveActiveTitle();
    });

    m.def("get_active_title_id", []() -> uint32_t {
        return static_cast<uint32_t>(GW::player::GetActiveTitleId());
    });

    m.def("deposit_faction", [](uint32_t allegiance) -> bool {
        return GW::player::DepositFaction(allegiance);
    }, py::arg("allegiance"));

    m.def("get_player_agent_id", [](uint32_t player_id) -> uint32_t {
        return GW::player::GetPlayerAgentId(player_id);
    }, py::arg("player_id"));

    m.def("get_amount_of_players_in_instance", []() -> uint32_t {
        return GW::player::GetAmountOfPlayersInInstance();
    });

    m.def("get_player_number", []() -> uint32_t {
        return GW::player::GetPlayerNumber();
    });

    m.def("get_player_name", [](uint32_t player_id) -> std::string {
        return WideToStr(GW::player::GetPlayerName(player_id));
    }, py::arg("player_id") = 0);

    m.def("set_player_name", [](uint32_t player_id, const std::string& name) -> std::string {
        auto wname = StrToWide(name);
        auto* result = GW::player::SetPlayerName(player_id, wname.c_str());
        return WideToStr(result);
    }, py::arg("player_id"), py::arg("name"));

    m.def("change_second_profession", [](uint32_t profession, uint32_t hero_index) -> bool {
        return GW::player::ChangeSecondProfession(
            static_cast<GW::Constants::Profession>(profession), hero_index);
    }, py::arg("profession"), py::arg("hero_index") = 0);

    m.def("get_title_ids", []() -> py::list {
        auto ids = GW::player::GetTitleIDs();
        py::list result;
        for (auto id : ids) result.append(id);
        return result;
    });
}
