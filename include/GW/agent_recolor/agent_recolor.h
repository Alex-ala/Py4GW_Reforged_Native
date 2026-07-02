#pragma once

#include "base/error_handling.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

// AgentRecolor (legacy AgentTagColor, py_agent_tag_color.h; renamed on request)
// ----------------------------------------------------------------------------
// Recolors agent overhead name tags (and the shared consider/target ring) by
// detouring the native color RESOLVER the game itself re-reads on every agent
// update. RE closeout: docs/RE/name_tag_color_reverse_engineering.md (legacy).
//
// Hook target: the RESOLVER GetConsiderColor (EXE FUN_007f02e0, __thiscall) —
// what the game's name-tag path (CCharAgent::GetTextData) and the consider ring
// call directly with the CCharAgent view pointer. The color is a 4-byte ARGB
// written THROUGH the out pointer. The detour runs the original first, recovers
// the agent id from view+0x2C, then overwrites *out for agents matching a rule.
// Color is ARGB 0xAARRGGBB (opaque red = 0xFFFF0000).
//
// NOTE: the id-addressable wrapper AvCharGetConsiderColor (FUN_007d9cf0) is NOT
// on the game's render path — hooking it recolors nothing. We only use it as a
// build-portable ANCHOR: the "agent" assertion at AvApi.cpp:0x1e9 ->
// to_function_start gives the wrapper, whose body CALLs ManagerFindChar (+0x07)
// and the resolver (+0x31); we derive both from there and hook the resolver.
// Scan inputs live in offsets/agent_recolor.json.

namespace GW::agent_recolor {

bool Initialize();
void Shutdown();

// GetConsiderColor RESOLVER (EXE FUN_007f02e0), __thiscall, emulated as
// __fastcall so a free-function detour matches the thiscall ABI: `this`
// arrives in ECX (fastcall arg 1), EDX is unused (fastcall arg 2), and the two
// stack args (out, flag) follow. RET 8 <-> fastcall cleans 8.
using ResolverFn = uint32_t*(__fastcall*)(void* view, void* edx, uint32_t* out, int flag);
// ManagerFindChar (EXE FUN_007fc920), __cdecl: agent_id -> CCharAgent view or null.
using FindCharFn = void*(__cdecl*)(uint32_t agent_id);

// Module-owned resolved symbols.
extern ResolverFn g_resolver;           // hooked function (-> detour)
extern ResolverFn g_resolver_original;  // trampoline -> original behavior
extern FindCharFn g_find_char;          // id -> view (reads + null guard)

// Module-owned resolvers (bodies in agent_recolor_patterns.cpp). Both derive
// from the AvCharGetConsiderColor wrapper anchor declared in
// offsets/agent_recolor.json ("agent_recolor.find_char_func" and
// "agent_recolor.consider_color_resolver_func").
bool ResolveFindCharFunction();
bool ResolveConsiderColorResolver();

class AgentRecolor {
public:
    struct Diagnostics {
        bool initialized = false;
        bool hook_installed = false;
        bool enabled = false;
        uint32_t resolver_calls_seen = 0;   // resolver calls observed while enabled
        uint32_t agent_rule_hits = 0;       // per-agent overrides applied
        uint32_t allegiance_rule_hits = 0;  // per-allegiance overrides applied
        uint32_t last_agent_id = 0;
        uint32_t last_color = 0;            // last color returned by the detour (ARGB)
    };

    static AgentRecolor& Instance();

    // Resolve + install the resolver detour. Safe to call once at DLL init.
    void Initialize();
    void Terminate();

    void Enable();
    void Disable();
    bool IsEnabled() const;
    bool IsHookInstalled() const;

    // Rule store. Precedence: per-agent > per-allegiance > game default.
    // Colors are ARGB 0xAARRGGBB.
    void SetAgentColor(uint32_t agent_id, uint32_t argb);
    bool RemoveAgentColor(uint32_t agent_id);
    void SetAllegianceColor(int allegiance, uint32_t argb);   // allegiance 1..6
    bool RemoveAllegianceColor(int allegiance);
    void ClearRules();

    std::map<uint32_t, uint32_t> GetAgentRules() const;
    std::map<int, uint32_t> GetAllegianceRules() const;

    // Read-only: the color the game currently computes for `agent_id` (ARGB),
    // by calling the ORIGINAL (un-overridden) resolver. Returns 0 if the
    // resolver was not resolved or the agent is invalid.
    uint32_t ReadConsiderColor(uint32_t agent_id);

    Diagnostics GetDiagnostics() const;
    void ResetDiagnostics();

    // Detour entry point (called from the installed hook). Returns the color to
    // write to *out for this agent (default unchanged when disabled/no rule).
    uint32_t ApplyOverride(uint32_t agent_id, uint32_t resolved_color);

    struct RuleSnapshot {
        std::map<uint32_t, uint32_t> agent_rules;
        std::map<int, uint32_t> allegiance_rules;
    };

private:
    AgentRecolor() = default;
    AgentRecolor(const AgentRecolor&) = delete;
    AgentRecolor& operator=(const AgentRecolor&) = delete;

    void RebuildSnapshotLocked();

    mutable std::mutex rules_mutex_;
    std::map<uint32_t, uint32_t> agent_rules_;
    std::map<int, uint32_t> allegiance_rules_;
    std::shared_ptr<const RuleSnapshot> snapshot_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> hook_installed_{false};

    std::atomic<uint32_t> diag_resolver_calls_{0};
    std::atomic<uint32_t> diag_agent_hits_{0};
    std::atomic<uint32_t> diag_allegiance_hits_{0};
    std::atomic<uint32_t> diag_last_agent_{0};
    std::atomic<uint32_t> diag_last_color_{0};
};

}  // namespace GW::agent_recolor
