#pragma once

#include "base/error_handling.h"

#include "base/scanner.h"

#include <filesystem>
#include <string>

namespace py4gw {

struct PatternObject {
    std::string name;
    std::string pattern;
    std::string mask;
    std::string assertion_file;
    std::string assertion_message;
    int offset = 0;
    int line_number = 0;
    int range = 0;
    ScannerSection section = ScannerSection::Text;
};

class Patterns {
public:
    static bool Initialize(const std::filesystem::path& directory = {});
    static const PatternObject* Get(const std::string& name);
};

}  // namespace py4gw
