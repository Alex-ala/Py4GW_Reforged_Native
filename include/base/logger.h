#pragma once

#include "base/error_handling.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class MessageType {
    Info,
    Warning,
    Error,
    Debug,
    Success,
    Performance,
    Notice,
    Hook
};

struct LoggerEntry {
    std::string timestamp;
    std::string display_timestamp;
    std::string module_name;
    std::string level;
    MessageType message_type;
    std::string message;
};

class Logger {
public:
    static Logger& Instance();

    bool LogInfo(const std::string& message, bool export_to_disk = true);
    static bool LogStaticInfo(const std::string& message) {
        return Instance().LogInfo(message);
    }
    bool LogWarning(const std::string& message, bool export_to_disk = true);
    bool LogError(const std::string& message, bool export_to_disk = true);
    bool LogError(const std::string& message, const std::string& module_name, bool export_to_disk = true);
    bool LogOk(const std::string& message, bool export_to_disk = true);
    bool LogDebug(const std::string& message, bool export_to_disk = true);
    bool LogNotice(const std::string& message, bool export_to_disk = true);
    bool LogPerformance(const std::string& message, bool export_to_disk = true);
    bool LogHook(const std::string& message, bool export_to_disk = true);
    bool Log(const std::string& module_name, const std::string& level, const std::string& message, bool export_to_disk = true);
    bool Log(const std::string& module_name, MessageType message_type, const std::string& message, bool export_to_disk = true);
    static bool AssertAddress(const std::string& name, uintptr_t address);
    static bool AssertAddress(const std::string& name, uintptr_t address, const std::string& module_name);
    static bool AssertHook(const std::string& name, int status);
    static bool AssertHook(const std::string& name, int status, const std::string& module_name);

    static const std::unordered_map<std::string, uintptr_t>& GetScanResults();
    static const std::unordered_map<std::string, int>& GetHookResults();
    std::vector<LoggerEntry> GetEntries() const;
    void ClearEntries();

    void SetLogFile(const std::string& file_path);

private:
    int max_entries = 1000;
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex log_mutex_;
    std::string log_file_path_;
    std::vector<LoggerEntry> entries_;

    std::string GetTimestamp(const char* format) const;
    static const char* MessageTypeToLevel(MessageType message_type);
    static MessageType LevelToMessageType(const std::string& level);
    bool WriteLog(const std::string& module_name, const std::string& level, MessageType message_type, const std::string& message, bool export_to_disk);
};
