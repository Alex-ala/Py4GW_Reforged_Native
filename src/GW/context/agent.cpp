#include "base/error_handling.h"

#include "GW/context/agent.h"

namespace GW::Context {

AgentItem* Agent::GetAsAgentItem() {
    return GetIsItemType() ? static_cast<AgentItem*>(this) : nullptr;
}

AgentGadget* Agent::GetAsAgentGadget() {
    return GetIsGadgetType() ? static_cast<AgentGadget*>(this) : nullptr;
}

AgentLiving* Agent::GetAsAgentLiving() {
    return GetIsLivingType() ? static_cast<AgentLiving*>(this) : nullptr;
}

const AgentItem* Agent::GetAsAgentItem() const {
    return GetIsItemType() ? static_cast<const AgentItem*>(this) : nullptr;
}

const AgentGadget* Agent::GetAsAgentGadget() const {
    return GetIsGadgetType() ? static_cast<const AgentGadget*>(this) : nullptr;
}

const AgentLiving* Agent::GetAsAgentLiving() const {
    return GetIsLivingType() ? static_cast<const AgentLiving*>(this) : nullptr;
}

}  // namespace GW::Context
