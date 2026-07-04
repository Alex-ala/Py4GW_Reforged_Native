#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "listeners/listeners.h"
#include "GW/common/stoc.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

// Per-agent event capture, migrated and reworked from the legacy
// CombatEventQueue (py_combat_events.h). The seven captured packets are the
// server's per-agent state-change notifications - five are the AGENT_ATTR_UPDATE
// family (int/float, with/without target) plus skill activate/recharge - which
// is why this is "agent_events", not "combat".
//
// Realized as a Listener: Enable() installs the packet hooks, Disable() removes
// them, so when off there are no callbacks at all (zero overhead, zero crash
// surface). It is enabled by default at startup; disable it by name through the
// listener toggle surface. The hooks are deliberately "dumb": copy raw fields into
// a fixed ring buffer with sanity gates and do NO live game-state queries and NO
// stateful tracking. Python drains the buffer and does every lookup. This is the
// reshape from the legacy handlers, which queried agents on the game thread and
// crashed during map transitions (why the legacy module shipped disabled).

namespace PY4GW::listeners {

// Output event-type taxonomy pushed to Python in RawAgentEvent::event_type.
// These are the module's OWN codes (grouped by category), distinct from the
// wire-side GenericValueID values the handlers switch on. Single source of
// truth for the enum and the Python bindings.
#define GW_AGENT_EVENT_TYPES(X)        \
    X(SKILL_ACTIVATED,         1)      \
    X(ATTACK_SKILL_ACTIVATED,  2)      \
    X(SKILL_STOPPED,           3)      \
    X(SKILL_FINISHED,          4)      \
    X(ATTACK_SKILL_FINISHED,   5)      \
    X(INTERRUPTED,             6)      \
    X(INSTANT_SKILL_ACTIVATED, 7)      \
    X(ATTACK_SKILL_STOPPED,    8)      \
    X(ATTACK_STARTED,          13)     \
    X(ATTACK_STOPPED,          14)     \
    X(MELEE_ATTACK_FINISHED,   15)     \
    X(DISABLED,                16)     \
    X(KNOCKED_DOWN,            17)     \
    X(CASTTIME,                18)     \
    X(DAMAGE,                  30)     \
    X(CRITICAL,                31)     \
    X(ARMOR_IGNORING,          32)     \
    X(HEALING,                 33)     \
    X(CURRENT_HEALTH,          34)     \
    X(CURRENT_ENERGY,          35)     \
    X(HEALTH_REGEN_CHANGE,     36)     \
    X(ENERGY_REGEN_CHANGE,     37)     \
    X(REACHED_MAXHP,           38)     \
    X(EFFECT_APPLIED,          40)     \
    X(EFFECT_REMOVED,          41)     \
    X(EFFECT_ON_TARGET,        42)     \
    X(EFFECT_RENEWED,          43)     \
    X(ENERGY_GAINED,           50)     \
    X(ENERGY_SPENT,            51)     \
    X(SKILL_DAMAGE,            60)     \
    X(SKILL_ACTIVATE_PACKET,   70)     \
    X(SKILL_RECHARGE,          80)     \
    X(SKILL_RECHARGED,         81)

enum class AgentEventType : uint32_t {
#define GW_AGENT_EVENT_ENUM(name, value) name = value,
    GW_AGENT_EVENT_TYPES(GW_AGENT_EVENT_ENUM)
#undef GW_AGENT_EVENT_ENUM
};

constexpr uint32_t to_uint(AgentEventType type) {
    return static_cast<uint32_t>(type);
}

// A single captured event. POD so it lives in the preallocated ring buffer
// with no per-event allocation. Field meanings vary by event_type. The
// max_hp/max_energy fields are carried for wire compatibility with the legacy
// struct but are not populated by the capture layer.
struct RawAgentEvent {
    uint64_t timestamp = 0;   // System::GetTickCount64() when captured
    uint32_t event_type = 0;  // AgentEventType
    uint32_t agent_id = 0;    // primary agent (caster/attacker/target by event)
    uint32_t value = 0;       // skill id, effect id, or other uint value
    uint32_t target_id = 0;   // secondary agent (target of skill/attack)
    float float_value = 0.0f; // duration, damage fraction, energy, etc.
    uint32_t agent_max_hp = 0;
    uint32_t agent_max_energy = 0;
    uint32_t target_max_hp = 0;
    uint32_t target_max_energy = 0;

    RawAgentEvent() = default;
    RawAgentEvent(uint64_t ts, uint32_t type, uint32_t agent, uint32_t val, uint32_t target, float fval)
        : timestamp(ts), event_type(type), agent_id(agent), value(val), target_id(target), float_value(fval) {}
};

// Captures the per-agent notification packets into a fixed ring buffer while
// enabled. Enabled by default at startup (inherits Listener::EnabledByDefault).
class AgentEventsListener : public Listener {
public:
    static constexpr size_t kCapacity = 1000;

    const char* Name() const override { return "agent_events"; }

    // Buffer API drained by Python.
    std::vector<RawAgentEvent> GetAndClearEvents();
    std::vector<RawAgentEvent> PeekEvents() const;
    size_t GetEventCount() const;
    size_t GetCapacity() const { return kCapacity; }

protected:
    void Install() override;
    void Uninstall() override;

private:
    // Packet handlers - dumb capture only: no live game-state queries, no
    // stateful tracking.
    void OnSkillActivate(GW::Packet::StoC::SkillActivate* packet);
    void OnGenericValue(GW::Packet::StoC::GenericValue* packet);
    void OnGenericValueTarget(GW::Packet::StoC::GenericValueTarget* packet);
    void OnGenericFloat(GW::Packet::StoC::GenericFloat* packet);
    void OnGenericModifier(GW::Packet::StoC::GenericModifier* packet);
    void OnSkillRecharge(GW::Packet::StoC::SkillRecharge* packet);
    void OnSkillRecharged(GW::Packet::StoC::SkillRecharged* packet);

    // Copies one record into the ring buffer (thread-safe). Overwrites the
    // oldest entry when full.
    void Push(const RawAgentEvent& event);

    PY4GW::HookEntry skill_activate_entry_;
    PY4GW::HookEntry generic_value_entry_;
    PY4GW::HookEntry generic_value_target_entry_;
    PY4GW::HookEntry generic_float_entry_;
    PY4GW::HookEntry generic_modifier_entry_;
    PY4GW::HookEntry skill_recharge_entry_;
    PY4GW::HookEntry skill_recharged_entry_;

    mutable std::mutex buffer_mutex_;
    std::array<RawAgentEvent, kCapacity> buffer_{};
    size_t head_ = 0;   // next write slot
    size_t count_ = 0;  // valid entries, <= kCapacity
};

// Process-wide accessor (state reads + toggle), mirroring Merchant().
AgentEventsListener& AgentEvents();

}  // namespace PY4GW::listeners
