#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace PY4GW {

// INI-backed settings per docs/settings-ini-design.md. Documents are owned by
// SettingsManager and anchored inside the DLL-side settings tree:
//   Scope::Account -> settings/<email>/<name>  (staged until anchor resolves)
//   Scope::Global  -> settings/Global/<name>   (bound immediately, shared)
//   Scope::Root    -> <name>                   (module/project root, shared;
//                                               bound immediately). Reserved for
//                                               core files that must live at the
//                                               project root, e.g. Py4GW.ini.

enum class SettingsScope {
    Account,
    Global,
    Root
};

class IniFile {
public:
    // Typed getters never throw: missing key or unconvertible text returns
    // the caller's default.
    std::string GetString(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    bool GetBool(const std::string& section, const std::string& key, bool default_value = false) const;
    long long GetInt(const std::string& section, const std::string& key, long long default_value = 0) const;
    double GetFloat(const std::string& section, const std::string& key, double default_value = 0.0) const;

    // Setters take effect in memory immediately and mark the document dirty;
    // the manager autosave pump persists it.
    void SetString(const std::string& section, const std::string& key, const std::string& value);
    void SetBool(const std::string& section, const std::string& key, bool value);
    void SetInt(const std::string& section, const std::string& key, long long value);
    void SetFloat(const std::string& section, const std::string& key, double value);

    bool HasKey(const std::string& section, const std::string& key) const;
    std::vector<std::string> GetSections() const;
    std::vector<std::string> GetKeys(const std::string& section) const;
    bool DeleteKey(const std::string& section, const std::string& key);
    bool DeleteSection(const std::string& section);

    // Escape hatches; normal flow relies on the autosave pump.
    bool Save();
    bool Reload();

    bool IsDirty() const;
    bool IsBound() const;
    const std::string& Name() const;
    SettingsScope Scope() const;
    std::filesystem::path Path() const;

private:
    friend class SettingsManager;

    struct IniLine {
        enum class Kind { KeyValue, Comment, Raw };
        Kind kind = Kind::Raw;
        std::string key;
        std::string value;
        std::string raw;
    };
    struct IniSection {
        std::string name;  // empty name = preamble before the first header
        std::vector<IniLine> lines;
    };

    IniFile(std::string name, SettingsScope scope);
    IniFile(const IniFile&) = delete;
    IniFile& operator=(const IniFile&) = delete;

    void Bind(const std::filesystem::path& path);
    void AutosaveTick(uint64_t now_ms);

    bool LoadLocked();
    void SeedFromTemplateLocked();
    bool SaveLocked();
    std::string SerializeLocked() const;
    void ParseLocked(const std::string& content);
    void MarkDirtyLocked();

    const IniSection* FindSectionLocked(const std::string& section) const;
    IniSection& FindOrCreateSectionLocked(const std::string& section);
    const IniLine* FindKeyLocked(const std::string& section, const std::string& key) const;
    void SetValueLocked(const std::string& section, const std::string& key, const std::string& value);

    mutable std::mutex mutex_;
    std::string name_;
    SettingsScope scope_;
    std::filesystem::path path_;
    bool bound_ = false;
    bool dirty_ = false;
    uint64_t first_dirty_tick_ = 0;
    uint64_t last_change_tick_ = 0;
    std::vector<IniSection> sections_;
};

class SettingsManager {
public:
    static SettingsManager& Instance();

    // Returns the process-wide document for (name, scope); same pair always
    // yields the same document. Names are sanitized to a bare filename.
    IniFile& Open(const std::string& name, SettingsScope scope = SettingsScope::Account);

    // Stepped from the runtime update loop: binds account documents once the
    // account anchor resolves, then runs the debounced autosave pump.
    void Update();

    // Saves every dirty bound document; wired into shutdown.
    void FlushAll();

    // Copy config from THIS account's (name, Account) document into ANOTHER
    // account's document on disk (settings/<target_email>/<name>). The caller
    // reads its own live document; the values are overlaid onto the target's file
    // (existing target keys not present in the source are left untouched). The
    // target account's running client sees them on its next reload
    // (message-triggered or throttled) - this is a disk write, not a live
    // cross-process mutation. Returns false on a rejected email / save failure;
    // copying zero matching keys is a success. `keys` are matched verbatim (the
    // Python wrapper lowercases them to match on-disk key casing).
    //   ...Document -> the whole file (every section + key)
    //   ...Section  -> one section (all its keys)
    //   ...Keys     -> a named subset of keys within one section
    bool CopyDocumentToAccount(const std::string& name, const std::string& target_email);
    bool CopySectionToAccount(const std::string& name, const std::string& section,
                              const std::string& target_email);
    bool CopyKeysToAccount(const std::string& name, const std::string& section,
                           const std::vector<std::string>& keys,
                           const std::string& target_email);

    // Like Copy*, but the values are SUPPLIED by the caller (an explicit
    // key->value mapping - e.g. a saved profile or transformed settings) rather
    // than read from the caller's own document. Overlaid onto the target's
    // section in settings/<target_email>/<name>.
    bool ApplySectionToAccount(const std::string& name, const std::string& section,
                               const std::vector<std::pair<std::string, std::string>>& values,
                               const std::string& target_email);

private:
    // Shared writer: overlay (section, key, value) triples read from the caller's
    // own document onto settings/<target_email>/<name>, then save.
    bool WriteTriplesToAccount(
        const std::string& name,
        const std::vector<std::tuple<std::string, std::string, std::string>>& triples,
        const std::string& target_email);

    SettingsManager() = default;
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    std::mutex registry_mutex_;
    std::vector<std::unique_ptr<IniFile>> documents_;
};

}  // namespace PY4GW
