#include "base/error_handling.h"

#include "listeners/agent_events_listener.h"

#include "GW/common/constants/constants.h"
#include "GW/map/map.h"
#include "GW/stoc/stoc.h"
#include "system/system.h"

namespace PY4GW::listeners {

namespace StoCPacket = GW::Packet::StoC;

namespace {

// Sanity gate: skip capture during map transitions where game memory may be
// unstable. Reads map-context pointers only - no agent-array walks.
bool IsMapReady() {
    const auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded()
        && !GW::map::GetIsObserving()
        && instance_type != GW::Constants::InstanceType::Loading;
}

}  // namespace

AgentEventsListener& AgentEvents() {
    static AgentEventsListener instance;
    return instance;
}

void AgentEventsListener::Install() {
    GW::StoC::RegisterPacketCallback<StoCPacket::SkillActivate>(
        &skill_activate_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::SkillActivate* packet) { OnSkillActivate(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::GenericValue>(
        &generic_value_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::GenericValue* packet) { OnGenericValue(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::GenericValueTarget>(
        &generic_value_target_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::GenericValueTarget* packet) { OnGenericValueTarget(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::GenericFloat>(
        &generic_float_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::GenericFloat* packet) { OnGenericFloat(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::GenericModifier>(
        &generic_modifier_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::GenericModifier* packet) { OnGenericModifier(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::SkillRecharge>(
        &skill_recharge_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::SkillRecharge* packet) { OnSkillRecharge(packet); });

    GW::StoC::RegisterPacketCallback<StoCPacket::SkillRecharged>(
        &skill_recharged_entry_,
        [this](PY4GW::HookStatus*, StoCPacket::SkillRecharged* packet) { OnSkillRecharged(packet); });
}

void AgentEventsListener::Uninstall() {
    GW::StoC::RemoveCallback<StoCPacket::SkillActivate>(&skill_activate_entry_);
    GW::StoC::RemoveCallback<StoCPacket::GenericValue>(&generic_value_entry_);
    GW::StoC::RemoveCallback<StoCPacket::GenericValueTarget>(&generic_value_target_entry_);
    GW::StoC::RemoveCallback<StoCPacket::GenericFloat>(&generic_float_entry_);
    GW::StoC::RemoveCallback<StoCPacket::GenericModifier>(&generic_modifier_entry_);
    GW::StoC::RemoveCallback<StoCPacket::SkillRecharge>(&skill_recharge_entry_);
    GW::StoC::RemoveCallback<StoCPacket::SkillRecharged>(&skill_recharged_entry_);

    std::lock_guard<std::mutex> lock(buffer_mutex_);
    head_ = 0;
    count_ = 0;
}

void AgentEventsListener::Push(const RawAgentEvent& event) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_[head_] = event;
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) {
        ++count_;
    }
}

std::vector<RawAgentEvent> AgentEventsListener::GetAndClearEvents() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    std::vector<RawAgentEvent> result;
    result.reserve(count_);
    const size_t start = (head_ + kCapacity - count_) % kCapacity;
    for (size_t i = 0; i < count_; ++i) {
        result.push_back(buffer_[(start + i) % kCapacity]);
    }
    head_ = 0;
    count_ = 0;
    return result;
}

std::vector<RawAgentEvent> AgentEventsListener::PeekEvents() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    std::vector<RawAgentEvent> result;
    result.reserve(count_);
    const size_t start = (head_ + kCapacity - count_) % kCapacity;
    for (size_t i = 0; i < count_; ++i) {
        result.push_back(buffer_[(start + i) % kCapacity]);
    }
    return result;
}

size_t AgentEventsListener::GetEventCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return count_;
}

// ============================================================================
// Packet Handlers - dumb capture only
// ============================================================================
// Each handler copies raw packet fields into the ring buffer. No GetAgentByID
// or any other live game-state query, and no stateful tracking - Python does
// all lookups and interpretation. Sanity gates: non-null packet and a ready map
// (skip transitions where memory may be unstable). The enabled state is implied
// by installation: when disabled these callbacks are not registered.

void AgentEventsListener::OnSkillActivate(StoCPacket::SkillActivate* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }
    if (packet->skill_id == 0) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();
    Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_ACTIVATE_PACKET),
        packet->agent_id, packet->skill_id, 0, 0.0f));
}

void AgentEventsListener::OnGenericValue(StoCPacket::GenericValue* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();

    using namespace GW::Packet::StoC::GenericValueID;

    switch (packet->value_id) {
        case skill_activated:
            Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_ACTIVATED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_activated:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_SKILL_ACTIVATED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_stopped:
            Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_STOPPED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_finished:
            Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_FINISHED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_finished:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_SKILL_FINISHED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case interrupted:
            Push(RawAgentEvent(now, to_uint(AgentEventType::INTERRUPTED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case instant_skill_activated:
            Push(RawAgentEvent(now, to_uint(AgentEventType::INSTANT_SKILL_ACTIVATED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_stopped:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_SKILL_STOPPED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_stopped:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_STOPPED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case melee_attack_finished:
            Push(RawAgentEvent(now, to_uint(AgentEventType::MELEE_ATTACK_FINISHED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case disabled:
            // value=1 disabled (aftercast), value=0 can act.
            Push(RawAgentEvent(now, to_uint(AgentEventType::DISABLED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case add_effect:
            // Dumb capture: always EFFECT_APPLIED. The legacy applied-vs-renewed
            // distinction needed a stateful map on the game thread; Python now
            // derives renewal from the event stream if it needs it.
            Push(RawAgentEvent(now, to_uint(AgentEventType::EFFECT_APPLIED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case remove_effect:
            Push(RawAgentEvent(now, to_uint(AgentEventType::EFFECT_REMOVED),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_damage:
            Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_DAMAGE),
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case energygain:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ENERGY_GAINED),
                packet->agent_id, 0, 0, static_cast<float>(packet->value)));
            break;

        case max_hp_reached:
            Push(RawAgentEvent(now, to_uint(AgentEventType::REACHED_MAXHP),
                packet->agent_id, packet->value, 0, 0.0f));
            break;
    }
}

void AgentEventsListener::OnGenericValueTarget(StoCPacket::GenericValueTarget* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();

    using namespace GW::Packet::StoC::GenericValueID;

    // GWCA naming is swapped: packet->target is the caster, packet->caster is
    // the target (except effect_on_target, which uses normal naming). We do not
    // validate the ids here - Python resolves them.
    const uint32_t actual_caster = packet->target;
    const uint32_t actual_target = packet->caster;

    switch (packet->Value_id) {
        case skill_activated:
            Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_ACTIVATED),
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_skill_activated:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_SKILL_ACTIVATED),
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_started:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ATTACK_STARTED),
                actual_caster, 0, actual_target, 0.0f));
            break;

        case effect_on_target:
            // Normal naming here: caster applies, target receives.
            Push(RawAgentEvent(now, to_uint(AgentEventType::EFFECT_ON_TARGET),
                packet->caster, packet->value, packet->target, 0.0f));
            break;
    }
}

void AgentEventsListener::OnGenericFloat(StoCPacket::GenericFloat* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();

    using namespace GW::Packet::StoC::GenericValueID;
    // GWCA-unlabeled value_ids confirmed via WASM RE 2026-05-21.
    constexpr uint32_t energy = 33;               // AvCharNotifyStatInit(agent, ENERGY)
    constexpr uint32_t change_energy_regen = 43;  // AvCharNotifyStatRate(agent, ENERGY)

    switch (packet->type) {
        case knocked_down:
            Push(RawAgentEvent(now, to_uint(AgentEventType::KNOCKED_DOWN),
                packet->agent_id, 0, 0, packet->value));
            break;

        case casttime:
            Push(RawAgentEvent(now, to_uint(AgentEventType::CASTTIME),
                packet->agent_id, 0, 0, packet->value));
            break;

        case energy_spent:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ENERGY_SPENT),
                packet->agent_id, 0, 0, packet->value));
            break;

        case energy:
            Push(RawAgentEvent(now, to_uint(AgentEventType::CURRENT_ENERGY),
                packet->agent_id, 0, 0, packet->value));
            break;

        case health:
            Push(RawAgentEvent(now, to_uint(AgentEventType::CURRENT_HEALTH),
                packet->agent_id, 0, 0, packet->value));
            break;

        case change_energy_regen:
            Push(RawAgentEvent(now, to_uint(AgentEventType::ENERGY_REGEN_CHANGE),
                packet->agent_id, 0, 0, packet->value));
            break;

        case change_health_regen:
            Push(RawAgentEvent(now, to_uint(AgentEventType::HEALTH_REGEN_CHANGE),
                packet->agent_id, 0, 0, packet->value));
            break;
    }
}

void AgentEventsListener::OnGenericModifier(StoCPacket::GenericModifier* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();

    using namespace GW::Packet::StoC::GenericValueID;

    const uint32_t target_id = packet->target_id;  // receiver
    const uint32_t source_id = packet->cause_id;   // dealer
    const float value = packet->value;             // damage as fraction of max HP

    switch (packet->type) {
        case damage:
            Push(RawAgentEvent(now, to_uint(AgentEventType::DAMAGE),
                target_id, 0, source_id, value));
            break;

        case critical:
            Push(RawAgentEvent(now, to_uint(AgentEventType::CRITICAL),
                target_id, 0, source_id, value));
            break;

        case armorignoring:
            // Positive value is healing/lifesteal; non-positive is armor-ignoring damage.
            Push(RawAgentEvent(
                now,
                value > 0.0f ? to_uint(AgentEventType::HEALING) : to_uint(AgentEventType::ARMOR_IGNORING),
                target_id, 0, source_id, value));
            break;
    }
}

void AgentEventsListener::OnSkillRecharge(StoCPacket::SkillRecharge* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();
    Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_RECHARGE),
        packet->agent_id, packet->skill_id, 0, static_cast<float>(packet->recharge)));
}

void AgentEventsListener::OnSkillRecharged(StoCPacket::SkillRecharged* packet) {
    if (!packet || !IsMapReady()) {
        return;
    }

    const uint64_t now = PY4GW::System::GetTickCount64();
    Push(RawAgentEvent(now, to_uint(AgentEventType::SKILL_RECHARGED),
        packet->agent_id, packet->skill_id, 0, 0.0f));
}

}  // namespace PY4GW::listeners
