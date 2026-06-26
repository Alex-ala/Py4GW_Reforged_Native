#include "base/error_handling.h"

#include "base/logger.h"

#include "base/process_manager.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

static std::unordered_map<std::string, uintptr_t> s_scan_results;
static std::unordered_map<std::string, int> s_hook_results;

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

const std::unordered_map<std::string, uintptr_t>& Logger::GetScanResults() {
    return s_scan_results;
}

const std::unordered_map<std::string, int>& Logger::GetHookResults() {
    return s_hook_results;
}

std::vector<LoggerEntry> Logger::GetEntries() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(log_mutex_));
    return entries_;
}

void Logger::ClearEntries() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    entries_.clear();
}

void Logger::SetLogFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::filesystem::path path(file_path);
    if (path.is_relative()) {
        const auto process_dir = py4gw::process_manager::GetProcessDirectory();
        if (!process_dir.empty()) {
            path = process_dir / path;
        }
    }
    log_file_path_ = path.string();
}

bool Logger::LogInfo(const std::string& message) {
    return WriteLog("Py4GW", "INFO", message);
}

bool Logger::LogOk(const std::string& message) {
    return WriteLog("Py4GW", "SUCCESS", message);
}

bool Logger::LogHook(const std::string& message) {
    return WriteLog("Py4GW", "HOOK", message);
}

bool Logger::LogDebug(const std::string& message) {
    return WriteLog("Py4GW", "DEBUG", message);
}

bool Logger::LogNotice(const std::string& message) {
    return WriteLog("Py4GW", "NOTICE", message);
}

bool Logger::LogWarning(const std::string& message) {
    return WriteLog("Py4GW", "WARNING", message);
}

bool Logger::LogError(const std::string& message) {
    return WriteLog("Py4GW", "ERROR", message);
}

bool Logger::LogError(const std::string& message, const std::string& module_name) {
    return WriteLog(module_name, "ERROR", message);
}

bool Logger::Log(const std::string& module_name, const std::string& level, const std::string& message) {
    return WriteLog(module_name.empty() ? "Py4GW" : module_name, level, message);
}

bool Logger::AssertAddress(const std::string& name, uintptr_t address) {
    s_scan_results[name] = address;
    if (!address) {
        std::ostringstream oss;
        oss << name << " is null.";
        Logger::Instance().LogError(oss.str());
        return false;
    }
    return true;
}

bool Logger::AssertAddress(const std::string& name, uintptr_t address, const std::string& module_name) {
    s_scan_results[module_name + "::" + name] = address;
    if (!address) {
        std::ostringstream oss;
        oss << "[" << module_name << "] " << name << " is null.";
        Logger::Instance().LogError(oss.str());
        return false;
    }
    return true;
}

bool Logger::AssertHook(const std::string& name, int status) {
    s_hook_results[name] = status;
    if (status != 0) {
        std::ostringstream oss;
        oss << "Failed to hook " << name << ". MH_STATUS = " << status;
        Logger::Instance().LogError(oss.str());
        return false;
    }
    return true;
}

bool Logger::AssertHook(const std::string& name, int status, const std::string& module_name) {
    s_hook_results[module_name + "::" + name] = status;
    if (status != 0) {
        std::ostringstream oss;
        oss << "[" << module_name << "] Failed to hook " << name << ". MH_STATUS = " << status;
        Logger::Instance().LogError(oss.str());
        return false;
    }
    return true;
}

bool Logger::WriteLog(const std::string& module_name, const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    const std::string timestamp = GetTimestamp();
    const std::string line = timestamp + " [" + module_name + "] [" + level + "] " + message;
    entries_.push_back({ timestamp, module_name, level, message });
    if (entries_.size() > 100) {
        entries_.erase(entries_.begin(), entries_.begin() + (entries_.size() - 100));
    }

    try {
        if (!log_file_path_.empty()) {
            std::ofstream log_file(log_file_path_, std::ios::out | std::ios::app);
            if (log_file.is_open()) {
                log_file << line << std::endl;
                log_file.close();
            }
        }
        ::OutputDebugStringA((line + "\n").c_str());
    }
    catch (...) {
    }
    return !log_file_path_.empty();
}

std::string Logger::GetTimestamp() const {
    auto now = std::time(nullptr);
    std::tm time_info = {};
    localtime_s(&time_info, &now);
    std::ostringstream timestamp;
    timestamp << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
    return timestamp.str();
}
