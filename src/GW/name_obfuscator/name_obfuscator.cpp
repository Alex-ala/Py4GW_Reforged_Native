#include "base/error_handling.h"

#include "GW/name_obfuscator/name_obfuscator.h"

#include "GW/common/constants/constants.h"
#include "GW/common/opcodes.h"
#include "GW/context/guild.h"
#include "GW/map/map.h"
#include "GW/player/player.h"
#include "GW/stoc/stoc.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <unordered_set>

// Unmodeled StoC packets (no shared struct in GW/common/stoc.h). Field offsets include the +0
// header dword and match the wire layout. Layouts marked UNVERIFIED need a live byte-dump confirm
// (capture tools) before the matching surface is enabled — every handler is gated OFF by default
// for those. Declared module-locally, parity with the legacy py_name_obfuscator.cpp.
namespace GW {
    namespace Packet {
        namespace StoC {
            // Opcode 0x58 — second writer of the WC+0x80c name table (observer / instance-load).
            struct ClassObserver : Packet<ClassObserver> {
                /* +0x04 */ uint32_t player_number;
                /* +0x08 */ wchar_t  name[32]; // inline, NUL-terminated; mirrors 0x59 player_name[32]
            };

            // Account name: 0x30 (data) / 0x31 (update). Name @ +0x04 (Ghidra-confirmed). Width assumed 32.
            struct AcctNameData : Packet<AcctNameData> {
                /* +0x04 */ wchar_t  account_name[32];
            };

            // GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT (0x1DD). UNVERIFIED padding.
            struct PartySearchAdvertisement : Packet<PartySearchAdvertisement> {
                /* +0x04 */ uint8_t  pad04[0x1C];
                /* +0x20 */ wchar_t  message[32];      // 0x20..0x5F (32 wchars) -> ends at 0x60
                /* +0x60 */ wchar_t  party_leader[20];
            };

            // Score summary (1v1 opponent). Opcode 0xC0 (Ghidra-confirmed). name2 @ +0x110.
            struct ScoreSummary : Packet<ScoreSummary> {
                /* +0x04 */ uint8_t  pad04[0x0C];   // scores / container (unused here)
                /* +0x10 */ wchar_t  name1[122];    // +0x10..+0x103
                /* +0x104*/ uint8_t  gap[0x0C];     // pad to name2 @ +0x110
                /* +0x110*/ wchar_t  name2[122];
            };

            // Guild member char-name stream update. Opcode 0x129 (Ghidra-confirmed); name @ +0x04 wchar[20].
            struct GuildMemberUpdateCharName : Packet<GuildMemberUpdateCharName> {
                /* +0x04 */ wchar_t  char_name[20];
            };

            // 0x123 GuildInvite (GAME_SMSG_GUILD_INVITE_RECEIVED). guild name@+0x14, tag@+0x54, inviter@+0x60.
            struct GuildInvite : Packet<GuildInvite> {
                /* +0x04 */ uint8_t  pad04[0x10];
                /* +0x14 */ wchar_t  guild_name[32];   // +0x14..+0x53
                /* +0x54 */ wchar_t  guild_tag[6];     // +0x54..+0x5F
                /* +0x60 */ wchar_t  inviter_name[20];
            };

            // 0x12F GuildMotd. MOTD author player name @ +0x204 (motd text @ +0x04 left untouched).
            struct GuildMotd : Packet<GuildMotd> {
                /* +0x04 */ uint8_t  pad04[0x200];     // motd text wchar[256]
                /* +0x204*/ wchar_t  author[20];
            };

            // 0xAF OverrideName — local player's own name @ +0x04 → WorldCtx+0x68c (own-name chokepoint).
            struct OverrideName : Packet<OverrideName> {
                /* +0x04 */ wchar_t  name[32];
            };
            // NOTE: 0x121 GuildGeneral (guild name@+0x18, tag@+0x58) is modeled in GW/common/stoc.h;
            // 0x120 GuildDataAlly shares that exact layout and is handled by casting its PacketBase*
            // to GuildGeneral*.
        }
    }
}

namespace GW::name_obfuscator {

namespace {
    void OnPlayerJoinInstancePacket(PY4GW::HookStatus*, Packet::StoC::PlayerJoinInstance* pak) {
        NameObfuscator::Instance().OnPlayerJoinInstance(pak);
    }

    // Read a fixed inline buffer up to its NUL or capacity (never over-reads the field).
    std::wstring ReadBounded(const wchar_t* buf, size_t capacity) {
        if (!buf) {
            return {};
        }
        size_t n = 0;
        while (n < capacity && buf[n]) {
            ++n;
        }
        return std::wstring(buf, n);
    }
}

bool Initialize() {
    NameObfuscator::Instance().Initialize();
    return true;
}

void Shutdown() {
    NameObfuscator::Instance().Terminate();
}

NameObfuscator& NameObfuscator::Instance() {
    static NameObfuscator instance;
    return instance;
}

void NameObfuscator::Initialize() {
    if (initialized_.exchange(true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        RebuildAliasSnapshotLocked();
    }

    // Phase 1 — name table. 0x59 (modeled, existing) + 0x58 (raw, no shared define).
    GW::StoC::RegisterPacketCallback<Packet::StoC::PlayerJoinInstance>(
        &player_join_hook_, OnPlayerJoinInstancePacket, -0x8000);
    GW::StoC::RegisterPacketCallback(&class_observer_hook_, 0x58,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnClassObserver(b); },
        -0x8000);

    // Phase 2 — modeled GameSrv name carriers (default ON).
    GW::StoC::RegisterPacketCallback<Packet::StoC::MessageGlobal>(&message_global_hook_,
        [](PY4GW::HookStatus*, Packet::StoC::MessageGlobal* p) { NameObfuscator::Instance().OnMessageGlobal(p); }, -0x8000);
    GW::StoC::RegisterPacketCallback<Packet::StoC::ItemCustomisedForPlayer>(&item_custom_hook_,
        [](PY4GW::HookStatus*, Packet::StoC::ItemCustomisedForPlayer* p) { NameObfuscator::Instance().OnItemCustomised(p); }, -0x8000);
    GW::StoC::RegisterPacketCallback<Packet::StoC::MercenaryHeroInfo>(&mercenary_hook_,
        [](PY4GW::HookStatus*, Packet::StoC::MercenaryHeroInfo* p) { NameObfuscator::Instance().OnMercenaryInfo(p); }, -0x8000);
    GW::StoC::RegisterPacketCallback<Packet::StoC::GuildPlayerInfo>(&guild_info_hook_,
        [](PY4GW::HookStatus*, Packet::StoC::GuildPlayerInfo* p) { NameObfuscator::Instance().OnGuildPlayerInfo(p); }, -0x8000);

    // Phase 2 — opcode known, layout RE-sourced but UNVERIFIED (default OFF; capture-verify first).
    GW::StoC::RegisterPacketCallback(&party_search_hook_, GAME_SMSG_PARTY_SEARCH_ADVERTISEMENT,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnPartySearchAdvertisement(b); }, -0x8000);
    // Account name: 0x30 = data, 0x31 = update-name; both carry the name at +0x04 (opcodes Ghidra-confirmed).
    GW::StoC::RegisterPacketCallback(&acct_name_hook_, 0x30,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnAcctNameData(b); }, -0x8000);
    GW::StoC::RegisterPacketCallback(&acct_name_update_hook_, GAME_SMSG_HERO_ACCOUNT_NAME,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnAcctNameData(b); }, -0x8000);

    // Phase 4 — guild member char-name stream update (opcode 0x129, Ghidra-confirmed). Under surface_guild_ (ON).
    GW::StoC::RegisterPacketCallback(&guild_charname_hook_, 0x129,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnGuildMemberUpdateCharName(b); }, -0x8000);

    // ScoreSummary (opcode 0xC0, Ghidra-confirmed). Registered, but surface default OFF until the 1v1
    // mode-gate (IsScoreSummaryMaskable) is implemented — otherwise it would mask team/guild labels.
    GW::StoC::RegisterPacketCallback(&score_summary_hook_, 0xC0,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnScoreSummary(b); }, -0x8000);

    // §9 scope expansion — guild name/tag lane + own-name chokepoint (opcodes Ghidra-confirmed).
    GW::StoC::RegisterPacketCallback<Packet::StoC::GuildGeneral>(&guild_general_hook_,
        [](PY4GW::HookStatus*, Packet::StoC::GuildGeneral* p) { NameObfuscator::Instance().OnGuildGeneral(p); }, -0x8000);
    GW::StoC::RegisterPacketCallback(&guild_ally_hook_, GAME_SMSG_GUILD_ALLIANCE_INFO,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnGuildDataAlly(b); }, -0x8000);
    GW::StoC::RegisterPacketCallback(&guild_invite_hook_, GAME_SMSG_GUILD_INVITE_RECEIVED,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnGuildInvite(b); }, -0x8000);
    GW::StoC::RegisterPacketCallback(&guild_motd_hook_, 0x12F,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnGuildMotd(b); }, -0x8000);
    // Own-name chokepoint: OverrideName 0xAF writes WorldCtx+0x68c, read by self-gated coded resolvers.
    GW::StoC::RegisterPacketCallback(&override_name_hook_, 0xAF,
        [](PY4GW::HookStatus*, Packet::StoC::PacketBase* b) { NameObfuscator::Instance().OnOverrideName(b); }, -0x8000);

    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    diagnostics_.initialized = true;
    diagnostics_.player_join_hook_registered = true;
    diagnostics_.class_observer_hook_registered = true;
}

void NameObfuscator::Terminate() {
    if (!initialized_.exchange(false)) {
        return;
    }

    GW::StoC::RemoveCallbacks(&player_join_hook_);
    GW::StoC::RemoveCallbacks(&class_observer_hook_);
    GW::StoC::RemoveCallbacks(&message_global_hook_);
    GW::StoC::RemoveCallbacks(&item_custom_hook_);
    GW::StoC::RemoveCallbacks(&mercenary_hook_);
    GW::StoC::RemoveCallbacks(&guild_info_hook_);
    GW::StoC::RemoveCallbacks(&party_search_hook_);
    GW::StoC::RemoveCallbacks(&acct_name_hook_);
    GW::StoC::RemoveCallbacks(&acct_name_update_hook_);
    GW::StoC::RemoveCallbacks(&score_summary_hook_);
    GW::StoC::RemoveCallbacks(&guild_charname_hook_);
    GW::StoC::RemoveCallbacks(&guild_general_hook_);
    GW::StoC::RemoveCallbacks(&guild_ally_hook_);
    GW::StoC::RemoveCallbacks(&guild_invite_hook_);
    GW::StoC::RemoveCallbacks(&guild_motd_hook_);
    GW::StoC::RemoveCallbacks(&override_name_hook_);
    enabled_ = false;

    {
        std::lock_guard<std::mutex> lock(observed_mutex_);
        observed_players_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        aliases_.clear();
        alias_snapshot_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_ = {};
    }
}

void NameObfuscator::Enable() {
    enabled_ = true;
}

void NameObfuscator::Disable() {
    enabled_ = false;
}

bool NameObfuscator::IsEnabled() const {
    return enabled_.load();
}

bool NameObfuscator::IsMapReady() const {
    const auto instance_type = GW::map::GetInstanceType();
    return GW::map::GetIsMapLoaded()
        && !GW::map::GetIsObserving()
        && instance_type != GW::Constants::InstanceType::Loading;
}

void NameObfuscator::SetAlias(const std::wstring& real_name, const std::wstring& fake_name) {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    aliases_[real_name] = fake_name;
    RebuildAliasSnapshotLocked();
}

bool NameObfuscator::RemoveAlias(const std::wstring& real_name) {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    const auto erased = aliases_.erase(real_name) != 0;
    if (erased) {
        RebuildAliasSnapshotLocked();
    }
    return erased;
}

void NameObfuscator::ClearAliases() {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    aliases_.clear();
    RebuildAliasSnapshotLocked();
}

size_t NameObfuscator::AliasCount() const {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    return aliases_.size();
}

std::map<std::wstring, std::wstring> NameObfuscator::GetAliases() const {
    std::lock_guard<std::mutex> lock(alias_mutex_);
    return aliases_;
}

bool NameObfuscator::LookupReverse(const std::wstring& fake_name, std::wstring& real_out) const {
    std::shared_ptr<const AliasSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        snapshot = alias_snapshot_;
    }
    if (!snapshot) {
        return false;
    }
    const auto it = std::find_if(snapshot->begin(), snapshot->end(), [&fake_name](const AliasEntry& entry) {
        return entry.fake_name == fake_name;
    });
    if (it == snapshot->end()) {
        return false;
    }
    real_out = it->real_name;
    return true;
}

std::wstring NameObfuscator::ResolveRealName(const std::wstring& display_name) const {
    {
        std::lock_guard<std::mutex> lock(observed_mutex_);
        for (const auto& player : observed_players_) {
            if (player.display_name == display_name) {
                return player.real_name;
            }
        }
    }
    std::wstring real;
    if (LookupReverse(display_name, real)) {
        return real;
    }
    return display_name;
}

std::wstring NameObfuscator::ResolveDisplayName(const std::wstring& real_name) const {
    {
        std::lock_guard<std::mutex> lock(observed_mutex_);
        for (const auto& player : observed_players_) {
            if (player.real_name == real_name) {
                return player.display_name;
            }
        }
    }
    std::wstring fake;
    if (LookupAlias(real_name, fake)) {
        return fake;
    }
    return real_name;
}

std::atomic<bool>* NameObfuscator::SurfaceFlag(const std::string& surface) {
    if (surface == "class_observer") return &surface_class_observer_;
    if (surface == "message_global") return &surface_message_global_;
    if (surface == "item_custom")    return &surface_item_custom_;
    if (surface == "mercenary")      return &surface_mercenary_;
    if (surface == "guild")          return &surface_guild_;
    if (surface == "party_search")   return &surface_party_search_;
    if (surface == "acct_name")      return &surface_acct_name_;
    if (surface == "score_summary")  return &surface_score_summary_;
    if (surface == "guild_identity") return &surface_guild_identity_;
    if (surface == "own_name")       return &surface_own_name_;
    return nullptr;
}

const std::atomic<bool>* NameObfuscator::SurfaceFlag(const std::string& surface) const {
    return const_cast<NameObfuscator*>(this)->SurfaceFlag(surface);
}

bool NameObfuscator::SetSurfaceEnabled(const std::string& surface, bool on) {
    auto* flag = SurfaceFlag(surface);
    if (!flag) {
        return false;
    }
    flag->store(on);
    return true;
}

bool NameObfuscator::IsSurfaceEnabled(const std::string& surface) const {
    const auto* flag = SurfaceFlag(surface);
    return flag && flag->load();
}

std::vector<std::string> NameObfuscator::ListSurfaces() const {
    return {"class_observer", "message_global", "item_custom", "mercenary",
            "guild", "party_search", "acct_name", "score_summary",
            "guild_identity", "own_name"};
}

void NameObfuscator::ClearObservedCache() {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    observed_players_.clear();
}

size_t NameObfuscator::ObservedCount() const {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    return observed_players_.size();
}

std::vector<NameObfuscator::ObservedPlayer> NameObfuscator::GetObservedPlayers() const {
    std::lock_guard<std::mutex> lock(observed_mutex_);
    return observed_players_;
}

NameObfuscator::Diagnostics NameObfuscator::GetDiagnostics() const {
    const bool map_ready = IsMapReady(); // call GW API outside the lock (avoid lock-order inversion)
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    Diagnostics copy = diagnostics_;
    copy.enabled = enabled_.load();
    copy.current_map_ready = map_ready;
    copy.initialized = initialized_.load();
    copy.reverse_alias_collisions = reverse_alias_collisions_.load();
    return copy;
}

void NameObfuscator::ResetDiagnostics() {
    const bool initialized = initialized_.load();
    const bool enabled = enabled_.load();
    const bool current_map_ready = IsMapReady(); // GW API call outside the lock
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    diagnostics_ = {};
    diagnostics_.initialized = initialized;
    diagnostics_.player_join_hook_registered = initialized;
    diagnostics_.class_observer_hook_registered = initialized;
    diagnostics_.enabled = enabled;
    diagnostics_.current_map_ready = current_map_ready;
}

int NameObfuscator::ScrubGuildRoster() {
    // TODO(name-obfuscation P4): walk GuildClient::CMemberTable and clamp-overwrite the fixed +0x30
    // char-name buffer for aliased members. Needs live confirmation of the table pointer, member
    // stride, and +0x30 offset, plus a display-vs-CtoS field separation check before enabling.
    return 0;
}

int NameObfuscator::ScrubGuildIdentity() {
    if (!enabled_.load() || !surface_guild_identity_.load()) {
        return 0;
    }
    Context::GuildArray* guilds = Context::GetGuildArray();
    if (!guilds) {
        return 0;
    }
    int scrubbed = 0;
    for (Context::Guild* guild : *guilds) {
        if (!guild) {
            continue;
        }
        // name @ +0x30 wchar[32], tag @ +0x80 wchar[8] — fixed inline buffers, safe in-place rewrite.
        bool any = RewriteInlineName(guild->name, std::size(guild->name));
        any = RewriteInlineName(guild->tag, std::size(guild->tag)) || any;
        if (any) {
            ++scrubbed;
        }
    }
    if (scrubbed > 0) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_identity_hits += static_cast<uint32_t>(scrubbed);
    }
    return scrubbed;
}

bool NameObfuscator::LookupAlias(const std::wstring& real_name, std::wstring& fake_out) const {
    std::shared_ptr<const AliasSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(alias_mutex_);
        snapshot = alias_snapshot_;
    }
    if (!snapshot) {
        return false;
    }
    const auto it = std::find_if(snapshot->begin(), snapshot->end(), [&real_name](const AliasEntry& entry) {
        return entry.real_name == real_name;
    });
    if (it == snapshot->end()) {
        return false;
    }
    fake_out = it->fake_name;
    return true;
}

bool NameObfuscator::RewriteInlineName(wchar_t* buffer, size_t capacity, bool clamp_to_original,
                                       std::wstring* real_out, std::wstring* fake_out) {
    if (!buffer || capacity == 0 || !buffer[0]) {
        return false;
    }
    size_t orig_len = 0;
    while (orig_len < capacity && buffer[orig_len]) {
        ++orig_len;
    }
    if (orig_len == 0 || orig_len == capacity) {
        return false; // empty, or no NUL terminator within the field (malformed) -> leave untouched
    }
    const std::wstring real_name(buffer, orig_len);
    if (real_out) {
        *real_out = real_name;
    }
    std::wstring fake_name;
    if (!LookupAlias(real_name, fake_name)) {
        return false;
    }
    if (fake_out) {
        *fake_out = fake_name;
    }
    // clamp_to_original=false trusts a fixed-width array (modeled fields; proven safe by the
    // shipped 0x59 path). true also caps at the original length for variable/unverified layouts.
    size_t max_chars = capacity - 1;
    if (clamp_to_original) {
        max_chars = std::min(max_chars, orig_len);
    }
    const size_t to_copy = std::min(fake_name.size(), max_chars);
    std::memcpy(buffer, fake_name.data(), to_copy * sizeof(wchar_t));
    buffer[to_copy] = L'\0';
    return true;
}

bool NameObfuscator::IsLocalPlayerName(const std::wstring& name) const {
    if (name.empty()) {
        return false;
    }
    const wchar_t* local = player::GetPlayerName(0);
    return local && name == local;
}

bool NameObfuscator::IsScoreSummaryMaskable() const {
    // TODO(name-obfuscation P2): confirm the AreaInfo field that distinguishes 1v1 from team modes.
    // Until verified, never mask — team/guild labels must not be corrupted.
    return false;
}

void NameObfuscator::OnPlayerJoinInstance(Packet::StoC::PlayerJoinInstance* pak) {
    if (!pak) {
        return;
    }

    const bool map_ready = IsMapReady();
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.initialized = initialized_.load();
        diagnostics_.player_join_hook_registered = initialized_.load();
        diagnostics_.enabled = enabled_.load();
        diagnostics_.current_map_ready = map_ready;
        diagnostics_.player_packets_seen++;
    }

    if (!pak->player_name[0]) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_empty_name++;
        return;
    }

    if (!enabled_.load()) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_disabled++;
        return;
    }

    if (!map_ready) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.player_packets_map_not_ready++;
    }

    std::wstring real_name;
    std::wstring fake_name;
    const bool aliased = RewriteInlineName(pak->player_name, std::size(pak->player_name), false, &real_name, &fake_name);
    if (aliased) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.alias_hits++;
    }

    if (!real_name.empty()) {
        RecordObservedPlayer({pak->player_number, pak->agent_id, real_name, aliased ? fake_name : real_name, aliased});
    }
}

void NameObfuscator::OnClassObserver(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::ClassObserver*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_class_observer_.load()) {
        return;
    }

    std::wstring real_name;
    std::wstring fake_name;
    const bool aliased = RewriteInlineName(pak->name, std::size(pak->name), true, &real_name, &fake_name);
    if (real_name.empty()) {
        return;
    }
    if (aliased) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.class_observer_hits++;
    }

    RecordObservedPlayer({pak->player_number, 0, real_name, aliased ? fake_name : real_name, aliased});
}

void NameObfuscator::OnMessageGlobal(Packet::StoC::MessageGlobal* pak) {
    if (!pak || !enabled_.load() || !surface_message_global_.load()) {
        return;
    }
    if (RewriteInlineName(pak->sender_name, std::size(pak->sender_name))) {
        pak->sender_guild[0] = L'\0'; // don't leak the masked sender's guild tag
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.message_global_hits++;
    }
}

void NameObfuscator::OnItemCustomised(Packet::StoC::ItemCustomisedForPlayer* pak) {
    if (!pak || !enabled_.load() || !surface_item_custom_.load()) {
        return;
    }
    if (RewriteInlineName(pak->player_name, std::size(pak->player_name))) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.item_custom_hits++;
    }
}

void NameObfuscator::OnMercenaryInfo(Packet::StoC::MercenaryHeroInfo* pak) {
    if (!pak || !enabled_.load() || !surface_mercenary_.load()) {
        return;
    }
    const std::wstring name = ReadBounded(pak->name, std::size(pak->name));
    if (name.empty()) {
        return;
    }
    if (IsLocalPlayerName(name)) { // skip the local account's own mercenary clones
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.mercenary_self_skips++;
        return;
    }
    if (RewriteInlineName(pak->name, std::size(pak->name))) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.mercenary_hits++;
    }
}

void NameObfuscator::OnGuildPlayerInfo(Packet::StoC::GuildPlayerInfo* pak) {
    if (!pak || !enabled_.load() || !surface_guild_.load()) {
        return;
    }
    bool any = false;
    any = RewriteInlineName(pak->current_name, std::size(pak->current_name)) || any;
    any = RewriteInlineName(pak->invited_name, std::size(pak->invited_name)) || any;
    any = RewriteInlineName(pak->invited_by, std::size(pak->invited_by)) || any;
    if (any) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_info_hits++;
    }
}

void NameObfuscator::OnPartySearchAdvertisement(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::PartySearchAdvertisement*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_party_search_.load()) {
        return;
    }
    if (RewriteInlineName(pak->party_leader, std::size(pak->party_leader), true)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.party_search_hits++;
    }
}

void NameObfuscator::OnAcctNameData(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::AcctNameData*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_acct_name_.load()) {
        return;
    }
    const std::wstring name = ReadBounded(pak->account_name, std::size(pak->account_name));
    if (name.empty()) {
        return;
    }
    // Primary own-account protection is the alias map (you only alias other players). The char-name
    // compare is belt-and-suspenders; account name != char name, so it rarely matches.
    if (IsLocalPlayerName(name)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.acct_name_self_skips++;
        return;
    }
    if (RewriteInlineName(pak->account_name, std::size(pak->account_name), true)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.acct_name_hits++;
    }
}

void NameObfuscator::OnScoreSummary(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::ScoreSummary*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_score_summary_.load()) {
        return;
    }
    if (!IsScoreSummaryMaskable()) { // 1v1-only; team modes carry guild/team labels
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.score_summary_mode_skips++;
        return;
    }

    wchar_t* names[2] = {pak->name1, pak->name2};
    for (wchar_t* slot : names) {
        const std::wstring name = ReadBounded(slot, 122);
        if (name.empty()) {
            continue;
        }
        if (IsLocalPlayerName(name)) { // one of the two 1v1 names is always the local player
            std::lock_guard<std::mutex> lock(diagnostics_mutex_);
            diagnostics_.score_summary_self_skips++;
            continue;
        }
        if (RewriteInlineName(slot, 122, true)) {
            std::lock_guard<std::mutex> lock(diagnostics_mutex_);
            diagnostics_.score_summary_hits++;
        }
    }
}

void NameObfuscator::OnGuildMemberUpdateCharName(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::GuildMemberUpdateCharName*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_guild_.load()) {
        return;
    }
    if (RewriteInlineName(pak->char_name, std::size(pak->char_name), true)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_charname_hits++;
    }
}

void NameObfuscator::OnGuildGeneral(Packet::StoC::GuildGeneral* pak) {
    if (!pak || !enabled_.load() || !surface_guild_identity_.load()) {
        return;
    }
    // Guild name and tag both go through the alias map (alias a tag to "" to blank it).
    bool any = RewriteInlineName(pak->name, std::size(pak->name));
    any = RewriteInlineName(pak->tag, std::size(pak->tag)) || any;
    if (any) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_identity_hits++;
    }
}

void NameObfuscator::OnGuildDataAlly(Packet::StoC::PacketBase* base) {
    // 0x120 shares the 0x121 GuildGeneral layout (name@+0x18, tag@+0x58).
    OnGuildGeneral(base ? static_cast<Packet::StoC::GuildGeneral*>(base) : nullptr);
}

void NameObfuscator::OnGuildInvite(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::GuildInvite*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_guild_identity_.load()) {
        return;
    }
    bool any = RewriteInlineName(pak->guild_name, std::size(pak->guild_name), true);
    any = RewriteInlineName(pak->guild_tag, std::size(pak->guild_tag), true) || any;
    any = RewriteInlineName(pak->inviter_name, std::size(pak->inviter_name), true) || any;
    if (any) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_invite_hits++;
    }
}

void NameObfuscator::OnGuildMotd(Packet::StoC::PacketBase* base) {
    auto* pak = base ? static_cast<Packet::StoC::GuildMotd*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_guild_identity_.load()) {
        return;
    }
    if (RewriteInlineName(pak->author, std::size(pak->author), true)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.guild_motd_hits++;
    }
}

void NameObfuscator::OnOverrideName(Packet::StoC::PacketBase* base) {
    // Own-name chokepoint: rewrite the local player's name before it lands in WorldCtx+0x68c, so the
    // self-gated coded resolvers return the fake (nametag / target / call-follow chat sender).
    auto* pak = base ? static_cast<Packet::StoC::OverrideName*>(base) : nullptr;
    if (!pak || !enabled_.load() || !surface_own_name_.load()) {
        return;
    }
    if (RewriteInlineName(pak->name, std::size(pak->name), true)) {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        diagnostics_.own_name_hits++;
    }
}

void NameObfuscator::RebuildAliasSnapshotLocked() {
    auto snapshot = std::make_shared<AliasSnapshot>();
    snapshot->reserve(aliases_.size());
    std::unordered_set<std::wstring> seen_fakes;
    uint32_t collisions = 0;
    for (const auto& [real_name, fake_name] : aliases_) {
        snapshot->push_back({real_name, fake_name});
        if (!seen_fakes.insert(fake_name).second) {
            ++collisions; // two reals share one fake -> reverse lookup is ambiguous
        }
    }
    alias_snapshot_ = snapshot;
    reverse_alias_collisions_.store(collisions);
}

void NameObfuscator::RecordObservedPlayer(const ObservedPlayer& player) {
    std::unique_lock<std::mutex> lock(observed_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        std::lock_guard<std::mutex> diag_lock(diagnostics_mutex_);
        diagnostics_.observed_trylock_skips++;
        return;
    }

    auto it = std::find_if(observed_players_.begin(), observed_players_.end(), [&player](const ObservedPlayer& existing) {
        return existing.player_number == player.player_number
            || (player.agent_id != 0 && existing.agent_id == player.agent_id);
    });

    if (it != observed_players_.end()) {
        // A ClassObserver record carries agent_id==0; don't let it wipe an agent_id/number a prior
        // PlayerJoinInstance record already established for this player.
        const uint32_t kept_agent = (player.agent_id != 0) ? player.agent_id : it->agent_id;
        const uint32_t kept_number = (player.player_number != 0) ? player.player_number : it->player_number;
        *it = player;
        it->agent_id = kept_agent;
        it->player_number = kept_number;
    }
    else {
        observed_players_.push_back(player);
        if (observed_players_.size() > kMaxObservedPlayers) {
            observed_players_.erase(observed_players_.begin());
        }
    }

    std::lock_guard<std::mutex> diag_lock(diagnostics_mutex_);
    diagnostics_.observed_captures++;
}

}  // namespace GW::name_obfuscator
