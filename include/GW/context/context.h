#pragma once

#include "base/error_handling.h"

#include <cstdint>

namespace GW::Context {

struct AccountContext;
struct AgentContext;
struct CharContext;
struct GameplayContext;
struct GameContext;
struct GuildContext;
struct ItemContext;
struct MapContext;
struct PartyContext;
struct PreGameContext;
struct TradeContext;
struct WorldContext;
struct TextParser;

bool Initialize();
void Shutdown();

GameContext* GetGameContext();
PreGameContext* GetPreGameContext();
WorldContext* GetWorldContext();
PartyContext* GetPartyContext();
CharContext* GetCharContext();
GuildContext* GetGuildContext();
ItemContext* GetItemContext();
AgentContext* GetAgentContext();
MapContext* GetMapContext();
AccountContext* GetAccountContext();
TradeContext* GetTradeContext();
GameplayContext* GetGameplayContext();
TextParser* GetTextParser();
uint32_t GetControlledCharacterId();

}  // namespace GW::Context
