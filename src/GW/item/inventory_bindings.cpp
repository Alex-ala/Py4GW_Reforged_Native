#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "GW/game_thread/game_thread.h"
#include "GW/item/item.h"
#include "GW/map/map.h"
#include "GW/ui/ui.h"

namespace py = pybind11;

// PyInventory, migrated from legacy py_Inventory.cpp/.h. The legacy module
// mixed a SafeBag snapshot class with item operations that all exist in the
// function-based PyItem module already (use/equip/drop/pickup/move, gold,
// salvage_start, identify, destroy, xunlai, storage state) - those are NOT
// duplicated here. This module carries the inventory-specific remainder:
// bag snapshots (legacy SafeBag), the hovered item, the validated Salvage
// helper and the salvage-confirmation auto-accept.
namespace {

bool IsMapReady() {
    const auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving() && instance_type != GW::Constants::InstanceType::Loading;
}

// Snapshot of one bag (legacy SafeBag::GetContext), items as light dicts.
py::dict GetBagSnapshot(int bag_id) {
    py::dict out;
    out["id"] = bag_id;
    out["items_count"] = 0;
    out["container_item"] = 0;
    out["size"] = 0;
    out["is_inventory_bag"] = false;
    out["is_storage_bag"] = false;
    out["is_material_storage"] = false;
    py::list items;
    out["items"] = items;

    if (!IsMapReady()) {
        return out;
    }

    GW::Context::Bag* bag = GW::item::GetBag(static_cast<GW::Constants::Bag>(bag_id));
    if (!bag) {
        return out;
    }

    out["items_count"] = bag->items_count;
    out["container_item"] = bag->container_item;
    out["size"] = bag->items.size();
    out["is_inventory_bag"] = bag->IsInventoryBag();
    out["is_storage_bag"] = bag->IsStorageBag();
    out["is_material_storage"] = bag->IsMaterialStorage();

    for (size_t slot = 0; slot < bag->items.size(); ++slot) {
        GW::Context::Item* item = bag->items[slot];
        if (!item) {
            continue;
        }
        py::dict entry;
        entry["item_id"] = item->item_id;
        entry["slot"] = static_cast<uint32_t>(slot);
        entry["model_id"] = item->model_id;
        entry["quantity"] = item->quantity;
        items.append(entry);
    }
    return out;
}

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyInventory, m) {
    m.doc() = "Inventory access: bag snapshots and inventory-specific helpers. "
              "Item operations (use/equip/drop/move, gold, salvage_start, ...) "
              "live in PyItem.";

    m.def("get_bag", &GetBagSnapshot, py::arg("bag_id"),
        "Snapshot one bag by id (1-based, legacy GW::Constants::Bag). Returns a "
        "dict with bag flags, counts and an 'items' list of "
        "{item_id, slot, model_id, quantity} dicts.");

    m.def("get_hovered_item_id", []() -> uint32_t {
        GW::Context::Item* item = GW::item::GetHoveredItem();
        return item ? item->item_id : 0;
    }, "Item id currently hovered in the UI (0 if none).");

    m.def("salvage", [](uint32_t salv_kit_id, uint32_t item_id) {
        GW::Context::Item* item = GW::item::GetItemById(item_id);
        if (!item) return;
        GW::Context::Item* kit = GW::item::GetItemById(salv_kit_id);
        if (!kit) return;
        if (kit->IsSalvageKit() && item->IsSalvagable()) {
            GW::item::SalvageStart(kit->item_id, item->item_id);
        }
    }, py::arg("salv_kit_id"), py::arg("item_id"),
        "Validated salvage: starts salvage only if the kit is a salvage kit "
        "and the item is salvageable (legacy Inventory.Salvage).");

    m.def("accept_salvage_window", []() {
        // Auto accept "you can only salvage materials with a lesser salvage kit"
        GW::game_thread::Enqueue([] {
            GW::ui::ButtonClick(GW::ui::GetChildFrame(GW::ui::GetFrameByLabel(L"Game"), { 0x6, 0x62, 0x6 }));
        });
    }, "Auto-accept the lesser-salvage-kit confirmation dialog.");
}
