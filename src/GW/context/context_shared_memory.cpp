#include "base/error_handling.h"

#include "GW/shared_memory/segments.h"

#include "GW/context/account.h"
#include "GW/context/character.h"
#include "GW/context/context.h"
#include "GW/context/game.h"
#include "GW/context/gameplay.h"
#include "GW/context/item.h"
#include "GW/context/pregame.h"
#include "GW/context/render.h"
#include "GW/context/text_parser.h"
#include "GW/context/world.h"

// Contexts that have no dedicated GW module of their own, plus the pointer
// directory. One commentable line per struct.
namespace GW::shared_memory {

namespace {

// The pointer directory: address of every top-level context in one place, so a
// reader can resolve any context by name without publishing it as a full
// struct. (Full contexts published below carry their own inner pointers too.)
bool FillRuntimePointersSnapshot(Context::RuntimePointersSnapshot& snapshot, uint64_t) {
    snapshot = {};
    snapshot.mission_map_context = reinterpret_cast<uintptr_t>(Context::GetMissionMapContext());
    snapshot.world_map_context = reinterpret_cast<uintptr_t>(Context::GetWorldMapContext());
    snapshot.gameplay_context = reinterpret_cast<uintptr_t>(Context::GetGameplayContext());
    snapshot.instance_info = Context::GetInstanceInfoPtr();
    snapshot.map_context = reinterpret_cast<uintptr_t>(Context::GetMapContext());
    snapshot.game_context = reinterpret_cast<uintptr_t>(Context::GetGameContext());
    snapshot.pregame_context = reinterpret_cast<uintptr_t>(Context::GetPreGameContext());
    snapshot.world_context = reinterpret_cast<uintptr_t>(Context::GetWorldContext());
    snapshot.char_context = reinterpret_cast<uintptr_t>(Context::GetCharContext());
    snapshot.agent_context = reinterpret_cast<uintptr_t>(Context::GetAgentContext());
    snapshot.guild_context = reinterpret_cast<uintptr_t>(Context::GetGuildContext());
    snapshot.party_context = reinterpret_cast<uintptr_t>(Context::GetPartyContext());
    snapshot.trade_context = reinterpret_cast<uintptr_t>(Context::GetTradeContext());
    snapshot.item_context = reinterpret_cast<uintptr_t>(Context::GetItemContext());
    snapshot.friend_list = reinterpret_cast<uintptr_t>(Context::GetFriendList());
    snapshot.render_context = reinterpret_cast<uintptr_t>(Context::GetRenderContext());
    snapshot.text_parser = reinterpret_cast<uintptr_t>(Context::GetTextParser());
    snapshot.camera = reinterpret_cast<uintptr_t>(Context::GetCamera());
    snapshot.window_handle_ptr = Context::GetWindowHandlePtrAddress();
    return true;
}

}  // namespace

void RegisterContextSegments(Manager& manager) {
    manager.SubscribeSnapshot<Context::RuntimePointersSnapshot>("runtime.pointers", &FillRuntimePointersSnapshot, /*enabled=*/true);

    manager.SubscribeStruct<Context::CharContext>("runtime.ctx.char", &Context::GetCharContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::WorldContext>("runtime.ctx.world", &Context::GetWorldContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::GameContext>("runtime.ctx.game", &Context::GetGameContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::GameplayContext>("runtime.ctx.gameplay", &Context::GetGameplayContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::AccountContext>("runtime.ctx.account", &Context::GetAccountContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::PreGameContext>("runtime.ctx.pregame", &Context::GetPreGameContext, /*enabled=*/true);
    manager.SubscribeStruct<Context::TextParser>("runtime.ctx.text_parser", &Context::GetTextParser, /*enabled=*/true);
    manager.SubscribeStruct<Context::SalvageSessionInfo>("runtime.ctx.salvage", &Context::GetSalvageSessionInfo, /*enabled=*/true);
}

}  // namespace GW::shared_memory
