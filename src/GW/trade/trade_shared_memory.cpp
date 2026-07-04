#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/trade.h"

namespace GW::shared_memory {

void RegisterTradeSegments(Manager& manager) {
    manager.SubscribeStruct<Context::TradeContext>("runtime.ctx.trade", &Context::GetTradeContext, /*enabled=*/true);
}

}  // namespace GW::shared_memory
