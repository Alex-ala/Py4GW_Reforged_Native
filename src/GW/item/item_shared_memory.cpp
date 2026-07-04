#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/item.h"
#include "GW/context/world.h"

namespace GW::shared_memory {

void RegisterItemSegments(Manager& manager) {
    manager.SubscribeStruct<Context::ItemContext>("runtime.ctx.item", &Context::GetItemContext, /*enabled=*/true);

    manager.SubscribePointer("runtime.ptr.item_array", [] { return reinterpret_cast<uintptr_t>(Context::GetItemArray()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.inventory", [] { return reinterpret_cast<uintptr_t>(Context::GetInventory()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.item_formulas", [] { return reinterpret_cast<uintptr_t>(Context::GetItemFormulas()); }, /*enabled=*/true);
    manager.SubscribePointer("runtime.ptr.merchant_items", [] { return reinterpret_cast<uintptr_t>(Context::GetMerchantItemsArray()); }, /*enabled=*/true);
}

}  // namespace GW::shared_memory
