#include "base/error_handling.h"

#include "base/logger.h"

#include "base/process_manager.h"
#include "system/system.h"

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

void Logger::SetLogFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::filesystem::path path(file_path);
    if (path.is_relative()) {
        const auto process_dir = PY4GW::process_manager::GetProcessDirectory();
        if (!process_dir.empty()) {
            path = process_dir / path;
        }
    }
    log_file_path_ = path.string();
}

bool Logger::LogInfo(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "INFO", MessageType::Info, message, export_to_disk);
}

bool Logger::LogOk(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "SUCCESS", MessageType::Success, message, export_to_disk);
}

bool Logger::LogHook(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "HOOK", MessageType::Hook, message, export_to_disk);
}

bool Logger::LogDebug(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "DEBUG", MessageType::Debug, message, export_to_disk);
}

bool Logger::LogNotice(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "NOTICE", MessageType::Notice, message, export_to_disk);
}

bool Logger::LogPerformance(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "PERFORMANCE", MessageType::Performance, message, export_to_disk);
}

bool Logger::LogWarning(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "WARNING", MessageType::Warning, message, export_to_disk);
}

bool Logger::LogError(const std::string& message, bool export_to_disk) {
    return WriteLog("Py4GW", "ERROR", MessageType::Error, message, export_to_disk);
}

bool Logger::LogError(const std::string& message, const std::string& module_name, bool export_to_disk) {
    return WriteLog(module_name, "ERROR", MessageType::Error, message, export_to_disk);
}

bool Logger::Log(const std::string& module_name, const std::string& level, const std::string& message, bool export_to_disk) {
    const MessageType message_type = LevelToMessageType(level);
    return WriteLog(module_name.empty() ? "Py4GW" : module_name, MessageTypeToLevel(message_type), message_type, message, export_to_disk);
}

bool Logger::Log(const std::string& module_name, MessageType message_type, const std::string& message, bool export_to_disk) {
    return WriteLog(module_name.empty() ? "Py4GW" : module_name, MessageTypeToLevel(message_type), message_type, message, export_to_disk);
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

const char* Logger::MessageTypeToLevel(MessageType message_type) {
    switch (message_type) {
    case MessageType::Warning: return "WARNING";
    case MessageType::Error: return "ERROR";
    case MessageType::Debug: return "DEBUG";
    case MessageType::Success: return "SUCCESS";
    case MessageType::Performance: return "PERFORMANCE";
    case MessageType::Notice: return "NOTICE";
    case MessageType::Hook: return "HOOK";
    case MessageType::Info:
    default:
        return "INFO";
    }
}

MessageType Logger::LevelToMessageType(const std::string& level) {
    if (level == "ERROR") return MessageType::Error;
    if (level == "WARNING") return MessageType::Warning;
    if (level == "SUCCESS") return MessageType::Success;
    if (level == "DEBUG") return MessageType::Debug;
    if (level == "PERFORMANCE") return MessageType::Performance;
    if (level == "NOTICE") return MessageType::Notice;
    if (level == "HOOK") return MessageType::Hook;
    return MessageType::Info;
}

bool Logger::WriteFileLine(const std::string& module_name, const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    const std::string line = GetTimestamp("%Y-%m-%d %H:%M:%S") + " [" + module_name + "] [" + level + "] " + message;
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
    return true;
}

bool Logger::WriteLog(const std::string& module_name, const std::string& level, MessageType message_type, const std::string& message, bool export_to_disk) {
    PY4GW::System::Instance().AppendConsoleMessage(module_name, message_type, message);

    std::lock_guard<std::mutex> lock(log_mutex_);
    const std::string line = GetTimestamp("%Y-%m-%d %H:%M:%S") + " [" + module_name + "] [" + level + "] " + message;
    try {
        if (export_to_disk && !log_file_path_.empty()) {
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
    return true;
}

std::string Logger::GetTimestamp(const char* format) const {
    auto now = std::time(nullptr);
    std::tm time_info = {};
    localtime_s(&time_info, &now);
    std::ostringstream timestamp;
    timestamp << std::put_time(&time_info, format);
    return timestamp.str();
}
