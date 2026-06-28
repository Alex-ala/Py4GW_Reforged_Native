#include "base/error_handling.h"

#include "GW/context/account_context.h"
#include "GW/context/agent_context.h"
#include "GW/context/char_context.h"
#include "GW/context/context.h"
#include "GW/context/gameplay_context.h"
#include "GW/context/game_context.h"
#include "GW/context/guild_context.h"
#include "GW/context/item_context.h"
#include "GW/context/map_context.h"
#include "GW/context/party_context.h"
#include "GW/context/pregame_context.h"
#include "GW/context/text_parser.h"
#include "GW/context/trade_context.h"
#include "GW/context/world_context.h"

namespace GW::Context {

extern uintptr_t g_base_ptr;
extern uintptr_t g_pregame_context_addr;
extern uintptr_t g_gameplay_context_addr;

}  // namespace GW::Context

namespace GW::Context {

GameContext* GetGameContext() {
    auto** base_context = g_base_ptr ? *reinterpret_cast<uintptr_t***>(g_base_ptr) : nullptr;
    return base_context ? reinterpret_cast<GameContext*>(base_context[0x6]) : nullptr;
}

PreGameContext* GetPreGameContext() {
    return g_pregame_context_addr ? *reinterpret_cast<PreGameContext**>(g_pregame_context_addr) : nullptr;
}

WorldContext* GetWorldContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->world : nullptr;
}

PartyContext* GetPartyContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->party : nullptr;
}

CharContext* GetCharContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->character : nullptr;
}

GuildContext* GetGuildContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->guild : nullptr;
}

ItemContext* GetItemContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->items : nullptr;
}

AgentContext* GetAgentContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->agent : nullptr;
}

MapContext* GetMapContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->map : nullptr;
}

AccountContext* GetAccountContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->account : nullptr;
}

TradeContext* GetTradeContext() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->trade : nullptr;
}

GameplayContext* GetGameplayContext() {
    return g_gameplay_context_addr ? *reinterpret_cast<GameplayContext**>(g_gameplay_context_addr) : nullptr;
}

TextParser* GetTextParser() {
    GameContext* game_context = GetGameContext();
    return game_context ? game_context->text_parser : nullptr;
}

uint32_t GetControlledCharacterId() {
    WorldContext* world = GetWorldContext();
    return world && world->playerControlledChar ? world->playerControlledChar->agent_id : 0;
}

}  // namespace GW::Context
