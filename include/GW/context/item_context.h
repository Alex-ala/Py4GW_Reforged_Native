#pragma once

#include "base/error_handling.h"

#include "GW/common/gw_array.h"
#include "GW/context/item.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct InventoryTableEntry {
        uint32_t stride;
        uint32_t end;
        GW::Context::Inventory* start;
    };
    static_assert(sizeof(InventoryTableEntry) == 0xC, "InventoryTableEntry size mismatch");

    struct ItemContext { // total: 0x10C/268 BYTEs
        /* +h0000 */ GW::GWArray<void*> h0000;
        /* +h0010 */ GW::GWArray<void*> h0010;
        /* +h0020 */ uint32_t h0020;
        /* +h0024 */ GW::GWArray<Bag*> bags_array;
        /* +h0034 */ uint32_t h0034;
        /* +h0038 */ uint32_t h0038;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ GW::GWArray<void*> h0040;
        /* +h0050 */ GW::GWArray<void*> h0050;
        /* +h0060 */ uint32_t h0060;
        /* +h0064 */ uint32_t h0064;
        /* +h0068 */ uint32_t h0068;
        /* +h006C */ uint32_t h006C;
        /* +h0070 */ uint32_t h0070;
        /* +h0074 */ uint32_t h0074;
        /* +h0078 */ uint32_t h0078;
        /* +h007C */ uint32_t h007C;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ uint32_t h0084;
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ uint32_t h008C;
        /* +h0090 */ uint32_t h0090;
        /* +h0094 */ uint32_t h0094;
        /* +h0098 */ uint32_t h0098;
        /* +h009C */ uint32_t h009C;
        /* +h00A0 */ uint32_t h00A0;
        /* +h00A4 */ uint32_t h00A4;
        /* +h00A8 */ uint32_t h00A8;
        /* +h00AC */ uint32_t h00AC;
        /* +h00B0 */ uint32_t h00B0;
        /* +h00B4 */ uint32_t h00B4;
        /* +h00B8 */ GW::GWArray<Item*> item_array;
        /* +h00C8 */ uint32_t h00C8;
        /* +h00CC */ uint32_t h00CC;
        /* +h00D0 */ uint32_t h00D0;
        /* +h00D4 */ uint32_t h00D4;
        /* +h00D8 */ uint32_t h00D8;
        /* +h00DC */ uint32_t h00DC;
        /* +h00E0 */ uint32_t h00E0;
        /* +h00E4 */ GW::GWArray<InventoryTableEntry> inventory_table;
        /* +h00F4 */ uint32_t h00F4;
        /* +h00F8 */ Inventory* inventory;
        /* +h00FC */ GW::GWArray<void*> h00FC;
    };

    static_assert(offsetof(ItemContext, bags_array) == 0x24, "ItemContext::bags_array offset mismatch");
    static_assert(offsetof(ItemContext, item_array) == 0xB8, "ItemContext::item_array offset mismatch");
    static_assert(offsetof(ItemContext, inventory_table) == 0xE4, "ItemContext::inventory_table offset mismatch");
    static_assert(offsetof(ItemContext, inventory) == 0xF8, "ItemContext::inventory offset mismatch");
    static_assert(sizeof(ItemContext) == 0x10C, "ItemContext size mismatch");

}  // namespace GW::Context
