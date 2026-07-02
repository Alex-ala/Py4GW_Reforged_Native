#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/guild/guild.h"

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

PYBIND11_EMBEDDED_MODULE(PyGuild, m) {
    m.doc() = "Py4GW Guild bindings";

    m.def("get_player_guild_index", []() -> uint32_t {
        return GW::guild::GetPlayerGuildIndex();
    });

    m.def("get_player_guild_announcement", []() -> std::string {
        return WideToStr(GW::guild::GetPlayerGuildAnnouncement());
    });

    m.def("get_player_guild_announcer", []() -> std::string {
        return WideToStr(GW::guild::GetPlayerGuildAnnouncer());
    });

    m.def("travel_gh", []() -> bool {
        return GW::guild::TravelGH();
    });

    m.def("leave_gh", []() -> bool {
        return GW::guild::LeaveGH();
    });
}
