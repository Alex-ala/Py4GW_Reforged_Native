#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/quest/quest.h"

#include "GW/context/context.h"
#include "GW/context/quest.h"
#include "GW/context/world.h"
#include "GW/game_thread/game_thread.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace py = pybind11;

namespace {

// Faithful port of the legacy Quest class (Py4GW/include/py_quest.h). The legacy
// PyQuest was a set of static methods (== free functions); the reforged data half
// (QuestData / quest log) is intentionally read from the Python ctypes contexts
// (WorldContext.quest_log) and is NOT bound here. What IS bound: the game-side
// operations ctypes cannot do - chiefly the async string decoders
// (name/description/objectives/location/npc), which reuse the established item async
// pattern (per-id ready-map + std::thread + game_thread::Enqueue + module AsyncGet*).

// Sanitizing wide->utf8 conversion (legacy custom_WStringToString).
std::string WStringToUtf8(const std::wstring& s) {
    if (s.empty()) {
        return "Error In Name";
    }
    std::wstring clean;
    clean.reserve(s.size());
    for (wchar_t c : s) {
        if (c >= 32 || c == L'\n' || c == L'\r') {
            clean.push_back(c);
        }
    }
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, clean.c_str(), static_cast<int>(clean.size()),
                                                nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return "Error In Name";
    }
    std::string out(static_cast<size_t>(size_needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, clean.c_str(), static_cast<int>(clean.size()), out.data(), size_needed,
                        nullptr, nullptr);
    return out;
}

struct AsyncStrEntry {
    std::string value;
    bool ready = false;
};

// One store per decoded string field. Entries are pre-created on the calling thread by
// the request; the worker thread only writes to the already-existing entry.
std::unordered_map<int32_t, AsyncStrEntry> g_quest_name_map;
std::unordered_map<int32_t, AsyncStrEntry> g_quest_description_map;
std::unordered_map<int32_t, AsyncStrEntry> g_quest_objectives_map;
std::unordered_map<int32_t, AsyncStrEntry> g_quest_location_map;
std::unordered_map<int32_t, AsyncStrEntry> g_quest_npc_map;

bool IsAsyncReady(const std::unordered_map<int32_t, AsyncStrEntry>& store, int32_t quest_id) {
    const auto it = store.find(quest_id);
    return it != store.end() && it->second.ready;
}

std::string GetAsyncValue(const std::unordered_map<int32_t, AsyncStrEntry>& store, int32_t quest_id) {
    const auto it = store.find(quest_id);
    return it != store.end() ? it->second.value : std::string();
}

bool IsMissionMapQuestAvailable() {
    const auto* world_context = GW::Context::GetWorldContext();
    return world_context && world_context->mission_objectives.size() > 0;
}

using QuestDecodeFn = void (*)(const GW::Context::Quest*, std::wstring&);

// Normal-quest (quest_id >= 0) async string flow, shared by name/description/
// objectives/location/npc.
void RunNormalQuestString(int32_t quest_id, std::unordered_map<int32_t, AsyncStrEntry>& store,
                          QuestDecodeFn decode) {
    store[quest_id] = AsyncStrEntry{};  // pre-create on caller thread (ready=false)
    auto temp = std::make_shared<std::wstring>();
    std::thread([quest_id, &store, decode, temp]() {
        auto* quest = GW::quest::GetQuest(static_cast<GW::Constants::QuestID>(quest_id));
        if (!quest) {
            return;
        }
        GW::game_thread::Enqueue([quest, decode, temp]() { decode(quest, *temp); });
        const auto start = std::chrono::steady_clock::now();
        while (temp->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                    .count() >= 1000) {
                store[quest_id].value = "Timeout";
                store[quest_id].ready = true;
                return;
            }
        }
        store[quest_id].value = WStringToUtf8(*temp);
        store[quest_id].ready = true;
    }).detach();
}

void RequestQuestName(int32_t quest_id) {
    if (quest_id < 0) {
        g_quest_name_map[quest_id] =
            AsyncStrEntry{IsMissionMapQuestAvailable() ? "Mission Objectives" : "No Active Mission", true};
        return;
    }
    RunNormalQuestString(quest_id, g_quest_name_map, &GW::quest::AsyncGetQuestName);
}

void RequestQuestDescription(int32_t quest_id) {
    if (quest_id < 0) {
        g_quest_description_map[quest_id] =
            AsyncStrEntry{IsMissionMapQuestAvailable() ? "Mission Ongoing" : "No Active Mission", true};
        return;
    }
    RunNormalQuestString(quest_id, g_quest_description_map, &GW::quest::AsyncGetQuestDescription);
}

void RequestQuestNPC(int32_t quest_id) {
    if (quest_id < 0) {
        g_quest_npc_map[quest_id] =
            AsyncStrEntry{IsMissionMapQuestAvailable() ? "Mission Ongoing" : "No Active Mission", true};
        return;
    }
    RunNormalQuestString(quest_id, g_quest_npc_map, &GW::quest::AsyncGetQuestNPC);
}

void RequestQuestLocation(int32_t quest_id) {
    if (quest_id >= 0) {
        RunNormalQuestString(quest_id, g_quest_location_map, &GW::quest::AsyncGetQuestLocation);
        return;
    }
    g_quest_location_map[quest_id] = AsyncStrEntry{};
    if (!IsMissionMapQuestAvailable()) {
        g_quest_location_map[quest_id] = AsyncStrEntry{"No Active Mission", true};
        return;
    }
    auto temp = std::make_shared<std::wstring>();
    std::thread([quest_id, temp]() {
        auto* area_info = GW::map::GetCurrentMapInfo();
        if (!area_info) {
            return;
        }
        const uint32_t name_id = area_info->name_id ? area_info->name_id : 3;
        wchar_t encoded[8] = {};
        if (!GW::ui::UInt32ToEncStr(name_id, encoded, 8)) {
            return;
        }
        const std::wstring encoded_copy(encoded);
        GW::game_thread::Enqueue(
            [encoded_copy, temp]() { GW::quest::AsyncDecodeAnyEncStr(encoded_copy.c_str(), *temp); });
        const auto start = std::chrono::steady_clock::now();
        while (temp->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                    .count() >= 1000) {
                g_quest_location_map[quest_id].value = "Timeout";
                g_quest_location_map[quest_id].ready = true;
                return;
            }
        }
        g_quest_location_map[quest_id].value = WStringToUtf8(*temp);
        g_quest_location_map[quest_id].ready = true;
    }).detach();
}

void RequestQuestObjectives(int32_t quest_id) {
    if (quest_id >= 0) {
        RunNormalQuestString(quest_id, g_quest_objectives_map, &GW::quest::AsyncGetQuestObjectives);
        return;
    }
    g_quest_objectives_map[quest_id] = AsyncStrEntry{};
    std::thread([quest_id]() {
        if (!IsMissionMapQuestAvailable()) {
            g_quest_objectives_map[quest_id].value = "No Active Mission";
            g_quest_objectives_map[quest_id].ready = true;
            return;
        }
        const auto* world_context = GW::Context::GetWorldContext();
        if (!world_context) {
            return;
        }
        struct ObjJob {
            bool bullet = false;
            bool completed = false;
            std::shared_ptr<std::wstring> text;
        };
        std::vector<ObjJob> jobs;
        for (const auto& obj : world_context->mission_objectives) {
            if (!obj.enc_str) {
                continue;
            }
            const bool is_bullet = (obj.type & 0x1) != 0;
            const bool completed = (obj.type & 0x2) != 0;
            auto txt = std::make_shared<std::wstring>();
            const std::wstring enc_copy(obj.enc_str);
            GW::game_thread::Enqueue(
                [enc_copy, txt]() { GW::quest::AsyncDecodeAnyEncStr(enc_copy.c_str(), *txt); });
            jobs.push_back({is_bullet, completed, txt});
        }
        for (auto& j : jobs) {
            const auto start = std::chrono::steady_clock::now();
            while (j.text->empty() || *j.text == L"[PENDING]") {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                          start)
                        .count() >= 1000) {
                    *j.text = L"Timeout";
                    break;
                }
            }
        }
        std::wstring combined;
        combined.reserve(1024);
        for (auto& j : jobs) {
            if (j.bullet) {
                combined += (j.completed ? L"{sc}" : L"{s}");
            }
            combined += *j.text;
            combined += L"\n";
        }
        g_quest_objectives_map[quest_id].value = WStringToUtf8(combined);
        g_quest_objectives_map[quest_id].ready = true;
    }).detach();
}

bool IsQuestCompleted(int32_t quest_id) {
    if (quest_id < 0) {
        return false;
    }
    auto* quest = GW::quest::GetQuest(static_cast<GW::Constants::QuestID>(quest_id));
    return quest && quest->IsCompleted();
}

bool IsQuestPrimary(int32_t quest_id) {
    if (quest_id < 0) {
        return false;
    }
    auto* quest = GW::quest::GetQuest(static_cast<GW::Constants::QuestID>(quest_id));
    return quest && quest->IsPrimary();
}

std::vector<int32_t> GetQuestLogIds() {
    std::vector<int32_t> ids;
    auto* quest_log = GW::Context::GetQuestLog();
    if (!quest_log) {
        return ids;
    }
    for (auto& quest : *quest_log) {
        ids.push_back(static_cast<int32_t>(quest.quest_id));
    }
    return ids;
}

}  // namespace

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

    // --- ported from legacy PyQuest (operations ctypes cannot do) ---

    m.def("is_quest_completed", &IsQuestCompleted, py::arg("quest_id"));
    m.def("is_quest_primary", &IsQuestPrimary, py::arg("quest_id"));
    m.def("is_mission_map_quest_available", &IsMissionMapQuestAvailable);
    m.def("get_quest_log_ids", &GetQuestLogIds);

    m.def("request_quest_name", &RequestQuestName, py::arg("quest_id"));
    m.def("is_quest_name_ready", [](int32_t quest_id) { return IsAsyncReady(g_quest_name_map, quest_id); },
          py::arg("quest_id"));
    m.def("get_quest_name", [](int32_t quest_id) { return GetAsyncValue(g_quest_name_map, quest_id); },
          py::arg("quest_id"));

    m.def("request_quest_description", &RequestQuestDescription, py::arg("quest_id"));
    m.def("is_quest_description_ready",
          [](int32_t quest_id) { return IsAsyncReady(g_quest_description_map, quest_id); },
          py::arg("quest_id"));
    m.def("get_quest_description",
          [](int32_t quest_id) { return GetAsyncValue(g_quest_description_map, quest_id); },
          py::arg("quest_id"));

    m.def("request_quest_objectives", &RequestQuestObjectives, py::arg("quest_id"));
    m.def("is_quest_objectives_ready",
          [](int32_t quest_id) { return IsAsyncReady(g_quest_objectives_map, quest_id); },
          py::arg("quest_id"));
    m.def("get_quest_objectives",
          [](int32_t quest_id) { return GetAsyncValue(g_quest_objectives_map, quest_id); },
          py::arg("quest_id"));

    m.def("request_quest_location", &RequestQuestLocation, py::arg("quest_id"));
    m.def("is_quest_location_ready",
          [](int32_t quest_id) { return IsAsyncReady(g_quest_location_map, quest_id); },
          py::arg("quest_id"));
    m.def("get_quest_location", [](int32_t quest_id) { return GetAsyncValue(g_quest_location_map, quest_id); },
          py::arg("quest_id"));

    m.def("request_quest_npc", &RequestQuestNPC, py::arg("quest_id"));
    m.def("is_quest_npc_ready", [](int32_t quest_id) { return IsAsyncReady(g_quest_npc_map, quest_id); },
          py::arg("quest_id"));
    m.def("get_quest_npc", [](int32_t quest_id) { return GetAsyncValue(g_quest_npc_map, quest_id); },
          py::arg("quest_id"));
}
