#pragma once

#include "base/error_handling.h"

#include "base/hook_types.h"
#include "GW/common/stoc.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace GW::name_obfuscator {

bool Initialize();
void Shutdown();

// Player name obfuscator migrated from the legacy NameObfuscator
// (py_name_obfuscator.h). DLL initialization owns the StoC hooks; Python
// controls enable/disable, aliases, per-surface gates, and caches.
class NameObfuscator {
public:
    struct AliasEntry {
        std::wstring real_name;
        std::wstring fake_name;
    };

    using AliasSnapshot = std::vector<AliasEntry>;

    struct ObservedPlayer {
        uint32_t player_number = 0;
        uint32_t agent_id = 0;
        std::wstring real_name;
        std::wstring display_name;
        bool aliased = false;
    };

    struct Diagnostics {
        bool initialized = false;
        bool player_join_hook_registered = false;
        bool class_observer_hook_registered = false;
        bool enabled = false;
        bool current_map_ready = false;
        uint32_t player_packets_seen = 0;
        uint32_t player_packets_empty_name = 0;
        uint32_t player_packets_disabled = 0;
        uint32_t player_packets_map_not_ready = 0;
        uint32_t observed_captures = 0;
        uint32_t observed_trylock_skips = 0;
        uint32_t alias_hits = 0;
        // Per-surface alias hits / skips (Phase 1, 2, 4).
        uint32_t class_observer_hits = 0;
        uint32_t message_global_hits = 0;
        uint32_t item_custom_hits = 0;
        uint32_t mercenary_hits = 0;
        uint32_t mercenary_self_skips = 0;
        uint32_t guild_info_hits = 0;
        uint32_t party_search_hits = 0;
        uint32_t acct_name_hits = 0;
        uint32_t acct_name_self_skips = 0;
        uint32_t score_summary_hits = 0;
        uint32_t score_summary_mode_skips = 0;
        uint32_t score_summary_self_skips = 0;
        uint32_t guild_charname_hits = 0;
        uint32_t guild_identity_hits = 0;
        uint32_t guild_invite_hits = 0;
        uint32_t guild_motd_hits = 0;
        uint32_t own_name_hits = 0;
        uint32_t reverse_alias_collisions = 0;
    };

    static constexpr size_t kMaxObservedPlayers = 256;

    static NameObfuscator& Instance();

    void Initialize();
    void Terminate();

    void Enable();
    void Disable();
    bool IsEnabled() const;
    bool IsMapReady() const;

    void SetAlias(const std::wstring& real_name, const std::wstring& fake_name);
    bool RemoveAlias(const std::wstring& real_name);
    void ClearAliases();
    size_t AliasCount() const;
    std::map<std::wstring, std::wstring> GetAliases() const;

    // Reverse / display resolution (fake <-> real). Outbound-translation + tooling support.
    bool LookupReverse(const std::wstring& fake_name, std::wstring& real_out) const;
    std::wstring ResolveRealName(const std::wstring& display_name) const;
    std::wstring ResolveDisplayName(const std::wstring& real_name) const;

    // Per-surface enable flags (string-keyed). Master Enable()/Disable() still gates everything.
    bool SetSurfaceEnabled(const std::string& surface, bool on);
    bool IsSurfaceEnabled(const std::string& surface) const;
    std::vector<std::string> ListSurfaces() const;

    void ClearObservedCache();
    size_t ObservedCount() const;
    std::vector<ObservedPlayer> GetObservedPlayers() const;

    Diagnostics GetDiagnostics() const;
    void ResetDiagnostics();

    // Phase 4 fallback: scrub the durable guild member table for aliased members.
    int ScrubGuildRoster();
    // Scrub the live Guild struct (name @ +0x30, tag @ +0x80) for aliased guilds — needed because
    // 0x121/0x120 do not replay on travel, so already-loaded guild data won't be packet-rewritten.
    int ScrubGuildIdentity();

    bool LookupAlias(const std::wstring& real_name, std::wstring& fake_out) const;

    // Packet handlers. Modeled packets take their typed pointer; unmodeled packets
    // arrive as PacketBase* and are cast inside the .cpp (structs declared there).
    void OnPlayerJoinInstance(Packet::StoC::PlayerJoinInstance* pak);
    void OnClassObserver(Packet::StoC::PacketBase* pak);
    void OnMessageGlobal(Packet::StoC::MessageGlobal* pak);
    void OnItemCustomised(Packet::StoC::ItemCustomisedForPlayer* pak);
    void OnMercenaryInfo(Packet::StoC::MercenaryHeroInfo* pak);
    void OnGuildPlayerInfo(Packet::StoC::GuildPlayerInfo* pak);
    void OnPartySearchAdvertisement(Packet::StoC::PacketBase* pak);
    void OnAcctNameData(Packet::StoC::PacketBase* pak);
    void OnScoreSummary(Packet::StoC::PacketBase* pak);
    void OnGuildMemberUpdateCharName(Packet::StoC::PacketBase* pak);
    void OnGuildGeneral(Packet::StoC::GuildGeneral* pak);
    void OnGuildDataAlly(Packet::StoC::PacketBase* pak);
    void OnGuildInvite(Packet::StoC::PacketBase* pak);
    void OnGuildMotd(Packet::StoC::PacketBase* pak);
    void OnOverrideName(Packet::StoC::PacketBase* pak);

private:
    NameObfuscator() = default;
    NameObfuscator(const NameObfuscator&) = delete;
    NameObfuscator& operator=(const NameObfuscator&) = delete;

    void RebuildAliasSnapshotLocked();
    void RecordObservedPlayer(const ObservedPlayer& player);

    // Clamp+memcpy a fake name into an inline buffer. For fixed-width modeled fields clamp to
    // capacity-1 (allows a fake longer than the real name). For variable/unverified-layout packets
    // pass clamp_to_original=true to also cap at the original length, so a short inbound packet can
    // never be over-written. Returns true when an alias was applied; real_out/fake_out (optional)
    // are filled with the bounded original / applied fake.
    bool RewriteInlineName(wchar_t* buffer, size_t capacity, bool clamp_to_original = false,
                           std::wstring* real_out = nullptr, std::wstring* fake_out = nullptr);

    bool IsLocalPlayerName(const std::wstring& name) const;
    bool IsScoreSummaryMaskable() const;
    std::atomic<bool>* SurfaceFlag(const std::string& surface);
    const std::atomic<bool>* SurfaceFlag(const std::string& surface) const;

    mutable std::mutex alias_mutex_;
    std::map<std::wstring, std::wstring> aliases_;
    std::shared_ptr<const AliasSnapshot> alias_snapshot_;
    std::atomic<uint32_t> reverse_alias_collisions_ = 0;

    mutable std::mutex observed_mutex_;
    std::vector<ObservedPlayer> observed_players_;

    mutable std::mutex diagnostics_mutex_;
    Diagnostics diagnostics_;

    std::atomic<bool> initialized_ = false;
    std::atomic<bool> enabled_ = false;

    // Per-surface gates. Verified (RE/GWCA-modeled) default ON; unverified-layout default OFF.
    std::atomic<bool> surface_class_observer_ = true;
    std::atomic<bool> surface_message_global_ = true;
    std::atomic<bool> surface_item_custom_ = true;
    std::atomic<bool> surface_mercenary_ = true;
    std::atomic<bool> surface_guild_ = true;
    std::atomic<bool> surface_party_search_ = false;
    std::atomic<bool> surface_acct_name_ = false;
    std::atomic<bool> surface_score_summary_ = false;
    std::atomic<bool> surface_guild_identity_ = true;   // §9 scope expansion: guild name/tag
    std::atomic<bool> surface_own_name_ = true;         // §9.6: own-name masking now intended

    PY4GW::HookEntry player_join_hook_;
    PY4GW::HookEntry class_observer_hook_;
    PY4GW::HookEntry message_global_hook_;
    PY4GW::HookEntry item_custom_hook_;
    PY4GW::HookEntry mercenary_hook_;
    PY4GW::HookEntry guild_info_hook_;
    PY4GW::HookEntry party_search_hook_;
    PY4GW::HookEntry acct_name_hook_;
    PY4GW::HookEntry acct_name_update_hook_;
    PY4GW::HookEntry score_summary_hook_;
    PY4GW::HookEntry guild_charname_hook_;
    PY4GW::HookEntry guild_general_hook_;
    PY4GW::HookEntry guild_ally_hook_;
    PY4GW::HookEntry guild_invite_hook_;
    PY4GW::HookEntry guild_motd_hook_;
    PY4GW::HookEntry override_name_hook_;
};

}  // namespace GW::name_obfuscator
