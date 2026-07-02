#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/item/item.h"

#include <string>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyItem, m) {
    m.doc() = "Py4GW Item bindings";

    m.def("use_item_by_id", [](uint32_t item_id) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::UseItem(item);
    }, py::arg("item_id"));

    m.def("equip_item_by_id", [](uint32_t item_id, uint32_t agent_id) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::EquipItem(item, agent_id);
    }, py::arg("item_id"), py::arg("agent_id") = 0);

    m.def("drop_item_by_id", [](uint32_t item_id, uint32_t quantity) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::DropItem(item, quantity);
    }, py::arg("item_id"), py::arg("quantity"));

    m.def("pick_up_item_by_id", [](uint32_t item_id, uint32_t call_target) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::PickUpItem(item, call_target);
    }, py::arg("item_id"), py::arg("call_target") = 0);

    m.def("move_item", [](uint32_t item_id, int bag_id, uint32_t slot, uint32_t quantity) -> bool {
        auto* item = GW::item::GetItemById(item_id);
        if (!item) return false;
        return GW::item::MoveItem(item, static_cast<GW::Constants::Bag>(bag_id), slot, quantity);
    }, py::arg("item_id"), py::arg("bag_id"), py::arg("slot"), py::arg("quantity") = 0);

    m.def("use_item_by_model_id", [](uint32_t model_id, int bag_start, int bag_end) -> bool {
        return GW::item::UseItemByModelId(model_id, bag_start, bag_end);
    }, py::arg("model_id"), py::arg("bag_start") = 1, py::arg("bag_end") = 4);

    m.def("count_item_by_model_id", [](uint32_t model_id, int bag_start, int bag_end) -> uint32_t {
        return GW::item::CountItemByModelId(model_id, bag_start, bag_end);
    }, py::arg("model_id"), py::arg("bag_start") = 1, py::arg("bag_end") = 4);

    m.def("get_gold_amount_on_character", []() -> uint32_t {
        return GW::item::GetGoldAmountOnCharacter();
    });

    m.def("get_gold_amount_in_storage", []() -> uint32_t {
        return GW::item::GetGoldAmountInStorage();
    });

    m.def("drop_gold", [](uint32_t amount) -> bool {
        return GW::item::DropGold(amount);
    }, py::arg("amount") = 1);

    m.def("deposit_gold", [](uint32_t amount) -> uint32_t {
        return GW::item::DepositGold(amount);
    }, py::arg("amount") = 0);

    m.def("withdraw_gold", [](uint32_t amount) -> uint32_t {
        return GW::item::WithdrawGold(amount);
    }, py::arg("amount") = 0);

    m.def("change_gold", [](uint32_t character_gold, uint32_t storage_gold) -> bool {
        return GW::item::ChangeGold(character_gold, storage_gold);
    }, py::arg("character_gold"), py::arg("storage_gold"));

    m.def("salvage_start", [](uint32_t salvage_kit_id, uint32_t item_id) -> bool {
        return GW::item::SalvageStart(salvage_kit_id, item_id);
    }, py::arg("salvage_kit_id"), py::arg("item_id"));

    m.def("identify_item", [](uint32_t identification_kit_id, uint32_t item_id) -> bool {
        return GW::item::IdentifyItem(identification_kit_id, item_id);
    }, py::arg("identification_kit_id"), py::arg("item_id"));

    m.def("salvage_session_cancel", []() -> bool {
        return GW::item::SalvageSessionCancel();
    });

    m.def("salvage_session_done", []() -> bool {
        return GW::item::SalvageSessionDone();
    });

    m.def("destroy_item", [](uint32_t item_id) -> bool {
        return GW::item::DestroyItem(item_id);
    }, py::arg("item_id"));

    m.def("salvage_materials", []() -> bool {
        return GW::item::SalvageMaterials();
    });

    m.def("open_xunlai_window", [](bool anniversary_pane_unlocked) {
        GW::item::OpenXunlaiWindow(anniversary_pane_unlocked);
    }, py::arg("anniversary_pane_unlocked") = true);

    m.def("get_storage_page", []() -> int {
        return static_cast<int>(GW::item::GetStoragePage());
    });

    m.def("get_is_storage_open", []() -> bool {
        return GW::item::GetIsStorageOpen();
    });

    m.def("can_access_xunlai_chest", []() -> bool {
        return GW::item::CanAccessXunlaiChest();
    });

    m.def("get_material_storage_stack_size", []() -> uint32_t {
        return GW::item::GetMaterialStorageStackSize();
    });
}
