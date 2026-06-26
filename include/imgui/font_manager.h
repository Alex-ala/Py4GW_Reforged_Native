#pragma once

#include "base/error_handling.h"

#include <string>

struct ImFont;

namespace py4gw::imgui {

enum class FontId : int {
    Regular14,
    Regular22,
    Regular30,
    Regular46,
    Regular62,
    Regular124,
    Bold14,
    Bold22,
    Bold30,
    Bold46,
    Bold62,
    Bold124,
    Italic14,
    Italic22,
    Italic30,
    Italic46,
    Italic62,
    Italic124,
    BoldItalic14,
    BoldItalic22,
    BoldItalic30,
    BoldItalic46,
    BoldItalic62,
    BoldItalic124,
    Count
};

class FontManager {
public:
    static FontManager& Instance();

    bool Initialize();
    void Reset();
    ImFont* Get(FontId id);
    ImFont* GetDefaultFont() const;

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    ImFont* LoadFont(FontId id);
    bool MergeFontAwesome();
    bool ResolveFontDirectory();
    std::string BuildFontPath(const char* file_name) const;

    std::string font_dir_;
    ImFont* fonts_[static_cast<int>(FontId::Count)] = {};
    ImFont* default_font_ = nullptr;
    bool initialized_ = false;
};

}  // namespace py4gw::imgui
