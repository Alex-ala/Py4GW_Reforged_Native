#include "base/error_handling.h"

#include "base/patterns.h"

#include "base/process_manager.h"
#include "base/logger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using json = nlohmann::json;

std::unordered_map<std::string, py4gw::PatternObject> g_patterns;
bool g_initialized = false;

std::string QualifyName(const std::string& name_space, const std::string& name) {
    if (name_space.empty() || name.find('.') != std::string::npos) {
        return name;
    }
    return name_space + "." + name;
}

py4gw::ScannerSection ParseSection(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lowered == "rdata") {
        return py4gw::ScannerSection::RData;
    }
    if (lowered == "data") {
        return py4gw::ScannerSection::Data;
    }
    return py4gw::ScannerSection::Text;
}

bool ParseInt(const std::string& text, int* value) {
    if (!value) {
        return false;
    }

    try {
        size_t parsed = 0;
        const long long result = std::stoll(text, &parsed, 0);
        if (parsed != text.size() ||
            result < std::numeric_limits<int>::min() ||
            result > std::numeric_limits<int>::max()) {
            return false;
        }
        *value = static_cast<int>(result);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool DecodePatternLiteral(const std::string& literal, std::string* bytes) {
    if (!bytes) {
        return false;
    }

    bytes->clear();
    for (size_t i = 0; i < literal.size(); ++i) {
        if (literal[i] != '\\') {
            bytes->push_back(literal[i]);
            continue;
        }
        if (i + 1 >= literal.size()) {
            return false;
        }

        const char escape = literal[++i];
        switch (escape) {
        case '\\':
            bytes->push_back('\\');
            break;
        case '0':
            bytes->push_back('\0');
            break;
        case 'n':
            bytes->push_back('\n');
            break;
        case 'r':
            bytes->push_back('\r');
            break;
        case 't':
            bytes->push_back('\t');
            break;
        case 'x': {
            if (i + 2 >= literal.size()) {
                return false;
            }
            const char hi = literal[++i];
            const char lo = literal[++i];
            if (!std::isxdigit(static_cast<unsigned char>(hi)) || !std::isxdigit(static_cast<unsigned char>(lo))) {
                return false;
            }
            const std::string hex = { hi, lo };
            bytes->push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            break;
        }
        default:
            bytes->push_back(escape);
            break;
        }
    }

    return true;
}

bool LoadFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        Logger::Instance().LogError("Unable to open pattern file: " + path.string(), "patterns");
        return false;
    }

    json root;
    try {
        input >> root;
    }
    catch (const std::exception& ex) {
        Logger::Instance().LogError("Failed to parse " + path.string() + ": " + ex.what(), "patterns");
        return false;
    }

    if (!root.contains("patterns") || !root["patterns"].is_object()) {
        return true;
    }

    const std::string name_space = root.value("namespace", "");
    for (const auto& [raw_name, value] : root["patterns"].items()) {
        const std::string name = QualifyName(name_space, raw_name);
        if (g_patterns.contains(name)) {
            Logger::Instance().LogError("Duplicate pattern entry: " + name, "patterns");
            return false;
        }

        py4gw::PatternObject pattern_object;
        pattern_object.name = name;
        pattern_object.mask = value.value("mask", "");
        pattern_object.assertion_file = value.value("assertion_file", "");
        pattern_object.assertion_message = value.value("assertion_message", "");
        pattern_object.section = ParseSection(value.value("section", "text"));

        const bool has_pattern_field = value.contains("pattern");
        const bool has_mask_field = value.contains("mask");
        const bool has_assertion_file_field = value.contains("assertion_file");
        const bool has_assertion_message_field = value.contains("assertion_message");
        const bool has_offset_field = value.contains("offset");
        const bool has_line_field = value.contains("line_number");
        const bool has_range_field = value.contains("range");
        const bool has_section_field = value.contains("section");

        const std::string pattern_literal = value.value("pattern", "");
        const std::string offset_text = value.value("offset", "0");
        const std::string line_text = value.value("line_number", "0");
        const std::string range_text = value.value("range", "0");
        if ((!pattern_literal.empty() && !DecodePatternLiteral(pattern_literal, &pattern_object.pattern)) ||
            !ParseInt(offset_text, &pattern_object.offset) ||
            !ParseInt(line_text, &pattern_object.line_number) ||
            !ParseInt(range_text, &pattern_object.range)) {
            Logger::Instance().LogError("Invalid pattern entry: " + name, "patterns");
            return false;
        }

        const bool has_byte_pattern = has_pattern_field || has_mask_field;
        const bool has_assertion = has_assertion_file_field || has_assertion_message_field;
        const bool has_scan_config = has_offset_field || has_line_field || has_range_field || has_section_field;
        if (!has_byte_pattern && !has_assertion && !has_scan_config) {
            Logger::Instance().LogError("Pattern entry has no usable scanner inputs: " + name, "patterns");
            return false;
        }

        g_patterns.emplace(name, std::move(pattern_object));
    }

    return true;
}

}  // namespace

namespace py4gw {

bool Patterns::Initialize(const std::filesystem::path& directory) {
    if (g_initialized) {
        return true;
    }

    const std::filesystem::path pattern_directory = directory.empty()
        ? process_manager::GetModuleDirectory() / "offsets"
        : directory;
    if (pattern_directory.empty() || !std::filesystem::exists(pattern_directory)) {
        Logger::Instance().LogError("Pattern directory not found: " + pattern_directory.string(), "patterns");
        return false;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(pattern_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        Logger::Instance().LogError("No pattern JSON files found in: " + pattern_directory.string(), "patterns");
        return false;
    }

    g_patterns.clear();
    for (const auto& file : files) {
        if (!LoadFile(file)) {
            g_patterns.clear();
            return false;
        }
    }

    if (g_patterns.empty()) {
        Logger::Instance().LogError("Pattern initialization completed with zero loaded patterns.", "patterns");
        return false;
    }

    g_initialized = true;
    return true;
}

const PatternObject* Patterns::Get(const std::string& name) {
    if (!g_initialized) {
        return nullptr;
    }

    const auto it = g_patterns.find(name);
    if (it == g_patterns.end()) {
        return nullptr;
    }
    return &it->second;
}

}  // namespace py4gw
