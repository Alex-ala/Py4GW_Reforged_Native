#include "imgui/ext/ext.h"

#include <imgui.h>

#include "GW/textures/texture_manager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

// Implementation of the PyImGui.Ext composite widgets. Kept apart from the pybind
// registration (ext_bindings.cpp) so the ImGui/texture logic and the binding glue
// stay unmixed.

namespace PY4GW::ext {

namespace {

// UTF-8 -> UTF-16 for TextureManager keys (its cache is keyed by wide string).
std::wstring Widen(const std::string& s) {
    if (s.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

}  // namespace

void Ext::DrawItemTexture(const std::string& texture_path, float width, float height, bool disabled) {
    IDirect3DTexture9* tex = GW::textures::TextureManager::Instance().GetTexture(Widen(texture_path));
    if (!tex) {
        return;
    }
    const ImVec2 pad(width / 8.0f, height / 8.0f);
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rmax = ImGui::GetItemRectMax();
    const float w = (rmax.x - rmin.x) + 2.0f;
    const float h = (rmax.y - rmin.y) + 2.0f;
    const ImVec2 p0(rmin.x + pad.x, rmin.y + pad.y);
    const ImVec2 p1(p0.x + (w - pad.x * 2.0f), p0.y + (h - pad.y * 2.0f));
    const ImU32 tint = disabled ? IM_COL32(255, 255, 255, 155) : IM_COL32(255, 255, 255, 255);
    ImGui::GetWindowDrawList()->AddImage(
        static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex)), p0, p1,
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
}

bool Ext::ImageButtonImpl(const std::string& label, const std::string& texture_path, float width,
                          float height, bool disabled) {
    if (disabled) {
        ImGui::BeginDisabled();
    }
    // Hide the button's own text; the texture provides the visuals.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0, 0, 0, 0));
    const bool clicked = ImGui::Button(label.c_str(), ImVec2(width, height));
    ImGui::PopStyleColor(2);
    DrawItemTexture(texture_path, width, height, disabled);
    if (disabled) {
        ImGui::EndDisabled();
    }
    return clicked;
}

bool Ext::ImageButton(const std::string& label, const std::string& texture_path, float width, float height,
                      bool disabled) {
    return ImageButtonImpl(label, texture_path, width, height, disabled);
}

bool Ext::IconTile(const std::string& label, float x, float y, float width, float height,
                   const std::string& texture_path, bool disabled, const std::string& tooltip,
                   std::uint32_t overlay_fill, std::uint32_t overlay_outline) {
    ImGui::SetCursorPos(ImVec2(x, y));
    bool clicked;
    if (!texture_path.empty()) {
        clicked = ImageButtonImpl(label, texture_path, width, height, disabled);
    } else {
        if (disabled) {
            ImGui::BeginDisabled();
        }
        clicked = ImGui::Button(label.c_str(), ImVec2(width, height));
        if (disabled) {
            ImGui::EndDisabled();
        }
    }
    // "%s" guards against '%' in widget names being read as format specifiers.
    if (!tooltip.empty()) {
        ImGui::SetItemTooltip("%s", tooltip.c_str());
    }
    if (overlay_fill != 0u || overlay_outline != 0u) {
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (overlay_fill != 0u) {
            dl->AddRectFilled(rmin, rmax, overlay_fill, 3.0f);
        }
        if (overlay_outline != 0u) {
            dl->AddRect(rmin, rmax, overlay_outline, 3.0f, 0, 2.0f);
        }
    }
    return clicked;
}

}  // namespace PY4GW::ext
