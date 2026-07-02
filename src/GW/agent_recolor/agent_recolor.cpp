#include "base/error_handling.h"

#include "GW/agent_recolor/agent_recolor.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "GW/agent/agent.h"
#include "GW/context/agent.h"

namespace GW::agent_recolor {

ResolverFn g_resolver = nullptr;
ResolverFn g_resolver_original = nullptr;
FindCharFn g_find_char = nullptr;

namespace {
    int ResolveAgentAllegiance(uint32_t agent_id) {
        Context::Agent* agent = GW::agent::GetAgentByID(agent_id);
        if (!agent)
            return 0;
        Context::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living)
            return 0;
        return static_cast<int>(living->allegiance);
    }

    uint32_t* __fastcall Detour_GetConsiderColor(void* view, void* edx, uint32_t* out, int flag) {
        // Let the game resolve the default color first (writes through `out`).
        uint32_t* result = g_resolver_original ? g_resolver_original(view, edx, out, flag) : out;
        if (out && view) {
            // The agent id lives at view+0x2C on the CCharAgent view object.
            const uint32_t agent_id = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(view) + 0x2C);
            *out = AgentRecolor::Instance().ApplyOverride(agent_id, *out);
        }
        return result;
    }
}  // namespace

bool Initialize() {
    // Parity with legacy AgentTagColor: resolution or hook failure leaves the
    // module inert (colors stay game-default) instead of failing GW startup.
    AgentRecolor::Instance().Initialize();
    return true;
}

void Shutdown() {
    AgentRecolor::Instance().Terminate();
}

AgentRecolor& AgentRecolor::Instance() {
    static AgentRecolor instance;
    return instance;
}

void AgentRecolor::Initialize() {
    if (initialized_.exchange(true))
        return;

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        RebuildSnapshotLocked();
    }

    if (!ResolveFindCharFunction() || !ResolveConsiderColorResolver())
        return;

    CrashContextScope context("startup", "agent_recolor", "install_resolver_hook");
    const bool hook_ok = Logger::AssertHook(
        "GetConsiderColor_Resolver_Func",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&g_resolver),
            reinterpret_cast<void*>(&Detour_GetConsiderColor),
            reinterpret_cast<void**>(&g_resolver_original)),
        "agent_recolor");
    if (!hook_ok)
        return;

    PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_resolver));
    hook_installed_.store(true);
}

void AgentRecolor::Terminate() {
    if (!initialized_.exchange(false))
        return;

    CrashContextScope context("shutdown", "agent_recolor", "remove_resolver_hook");
    if (hook_installed_.exchange(false) && g_resolver) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_resolver));
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_resolver));
    }
    g_resolver = nullptr;
    g_resolver_original = nullptr;
    g_find_char = nullptr;
    enabled_.store(false);

    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    snapshot_.reset();
}

void AgentRecolor::Enable() {
    enabled_.store(true);
}

void AgentRecolor::Disable() {
    enabled_.store(false);
}

bool AgentRecolor::IsEnabled() const {
    return enabled_.load();
}

bool AgentRecolor::IsHookInstalled() const {
    return hook_installed_.load();
}

void AgentRecolor::SetAgentColor(uint32_t agent_id, uint32_t argb) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_[agent_id] = argb;
    RebuildSnapshotLocked();
}

bool AgentRecolor::RemoveAgentColor(uint32_t agent_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    const bool erased = agent_rules_.erase(agent_id) != 0;
    if (erased)
        RebuildSnapshotLocked();
    return erased;
}

void AgentRecolor::SetAllegianceColor(int allegiance, uint32_t argb) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    allegiance_rules_[allegiance] = argb;
    RebuildSnapshotLocked();
}

bool AgentRecolor::RemoveAllegianceColor(int allegiance) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    const bool erased = allegiance_rules_.erase(allegiance) != 0;
    if (erased)
        RebuildSnapshotLocked();
    return erased;
}

void AgentRecolor::ClearRules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    agent_rules_.clear();
    allegiance_rules_.clear();
    RebuildSnapshotLocked();
}

std::map<uint32_t, uint32_t> AgentRecolor::GetAgentRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return agent_rules_;
}

std::map<int, uint32_t> AgentRecolor::GetAllegianceRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return allegiance_rules_;
}

uint32_t AgentRecolor::ReadConsiderColor(uint32_t agent_id) {
    if (!g_find_char || !g_resolver_original)
        return 0;
    // Resolve the view ourselves. ManagerFindChar returns null for non-living /
    // non-char agents (items/gadgets/signposts) — the same check the game's
    // wrapper does, which avoids its crashing "charAgent" assertion. Then call
    // the ORIGINAL resolver (trampoline) directly with the view so the read
    // reflects the game DEFAULT, unaffected by any installed override.
    void* view = g_find_char(agent_id);
    if (!view)
        return 0;
    uint32_t color = 0;
    g_resolver_original(view, nullptr, &color, 1);  // flag 1 = name-tag text color
    return color;
}

AgentRecolor::Diagnostics AgentRecolor::GetDiagnostics() const {
    Diagnostics diag;
    diag.initialized = initialized_.load();
    diag.hook_installed = hook_installed_.load();
    diag.enabled = enabled_.load();
    diag.resolver_calls_seen = diag_resolver_calls_.load();
    diag.agent_rule_hits = diag_agent_hits_.load();
    diag.allegiance_rule_hits = diag_allegiance_hits_.load();
    diag.last_agent_id = diag_last_agent_.load();
    diag.last_color = diag_last_color_.load();
    return diag;
}

void AgentRecolor::ResetDiagnostics() {
    diag_resolver_calls_.store(0);
    diag_agent_hits_.store(0);
    diag_allegiance_hits_.store(0);
    diag_last_agent_.store(0);
    diag_last_color_.store(0);
}

uint32_t AgentRecolor::ApplyOverride(uint32_t agent_id, uint32_t resolved_color) {
    if (!enabled_.load(std::memory_order_relaxed))
        return resolved_color;

    std::shared_ptr<const RuleSnapshot> snap;
    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        snap = snapshot_;
    }

    diag_resolver_calls_.fetch_add(1, std::memory_order_relaxed);
    diag_last_agent_.store(agent_id, std::memory_order_relaxed);

    uint32_t out = resolved_color;
    if (snap) {
        const auto agent_it = snap->agent_rules.find(agent_id);
        if (agent_it != snap->agent_rules.end()) {
            out = agent_it->second;
            diag_agent_hits_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (!snap->allegiance_rules.empty()) {
            const int allegiance = ResolveAgentAllegiance(agent_id);
            const auto alleg_it = snap->allegiance_rules.find(allegiance);
            if (alleg_it != snap->allegiance_rules.end()) {
                out = alleg_it->second;
                diag_allegiance_hits_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    diag_last_color_.store(out, std::memory_order_relaxed);
    return out;
}

void AgentRecolor::RebuildSnapshotLocked() {
    auto snapshot = std::make_shared<RuleSnapshot>();
    snapshot->agent_rules = agent_rules_;
    snapshot->allegiance_rules = allegiance_rules_;
    snapshot_ = snapshot;
}

}  // namespace GW::agent_recolor
