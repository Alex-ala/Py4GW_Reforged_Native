#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/quest/quest.h"

#include <string>
#include <type_traits>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyQuest, m) {
    m.doc() = "Py4GW Quest bindings";

    m.def("set_active_quest_id", [](uint32_t quest_id) -> bool {
        return GW::quest::SetActiveQuestId(static_cast<GW::Constants::QuestID>(quest_id));
    }, py::arg("quest_id"));

    m.def("abandon_quest_id", [](uint32_t quest_id) -> bool {
        return GW::quest::AbandonQuestId(static_cast<GW::Constants::QuestID>(quest_id));
    }, py::arg("quest_id"));

    m.def("get_active_quest_id", []() -> uint32_t {
        using QuestIdUnderlying = std::underlying_type_t<GW::Constants::QuestID>;
        const auto quest_id = GW::Context::GetActiveQuestId();
        return static_cast<uint32_t>(static_cast<QuestIdUnderlying>(quest_id));
    });

    m.def("request_quest_info", [](uint32_t quest_id, bool update_markers) -> bool {
        return GW::quest::RequestQuestInfoId(static_cast<GW::Constants::QuestID>(quest_id), update_markers);
    }, py::arg("quest_id"), py::arg("update_markers") = false);

    m.def("get_quest_entry_group_name", [](uint32_t quest_id) -> std::string {
        wchar_t buffer[256] = {};
        if (GW::quest::GetQuestEntryGroupName(static_cast<GW::Constants::QuestID>(quest_id), buffer, 256)) {
            std::string result;
            for (int i = 0; buffer[i] && i < 256; ++i)
                result.push_back(static_cast<char>(buffer[i] < 128 ? buffer[i] : '?'));
            return result;
        }
        return {};
    }, py::arg("quest_id"));
}
