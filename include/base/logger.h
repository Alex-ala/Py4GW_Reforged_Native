#pragma once

#include "base/error_handling.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct LoggerEntry {
    std::string timestamp;
    std::string module_name;
    std::string level;
    std::string message;
};

class Logger {
public:
    static Logger& Instance();

    bool LogInfo(const std::string& message);
    static bool LogStaticInfo(const std::string& message) {
        return Instance().LogInfo(message);
    }
    bool LogWarning(const std::string& message);
    bool LogError(const std::string& message);
    bool LogError(const std::string& message, const std::string& module_name);
    bool LogOk(const std::string& message);
    bool LogDebug(const std::string& message);
    bool LogNotice(const std::string& message);
    bool LogHook(const std::string& message);
    bool Log(const std::string& module_name, const std::string& level, const std::string& message);
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
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex log_mutex_;
    std::string log_file_path_;
    std::vector<LoggerEntry> entries_;

    std::string GetTimestamp() const;
    bool WriteLog(const std::string& module_name, const std::string& level, const std::string& message);
};
