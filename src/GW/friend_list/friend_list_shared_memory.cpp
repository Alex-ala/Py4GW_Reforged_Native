#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/context.h"
#include "GW/context/friend_list.h"

namespace GW::shared_memory {

void RegisterFriendListSegments(Manager& manager) {
    manager.SubscribeStruct<Context::FriendList>("runtime.ctx.friends", &Context::GetFriendList, /*enabled=*/true);
}

}  // namespace GW::shared_memory
