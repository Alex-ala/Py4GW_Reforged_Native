#include "base/error_handling.h"

#include "GW/context/party.h"

namespace GW::Context {

size_t PartyInfo::GetPartySize() const {
    return players.size() + henchmen.size() + heroes.size();
}

}  // namespace GW::Context
