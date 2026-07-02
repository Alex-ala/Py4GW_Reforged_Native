#include "base/error_handling.h"

#include "imgui/addons/hotkey_demo.h"

#include <windows.h>

#include <imgui.h>

#include <array>
#include <vector>

namespace ImGui {

inline bool BeginChildFrame(ImGuiID id, const ImVec2& size, ImGuiWindowFlags extra_flags = 0) {
    return BeginChild(id, size, ImGuiChildFlags_FrameStyle, extra_flags);
}

inline void EndChildFrame() {
    EndChild();
}

struct LegacyWin32KeyState {
    std::array<bool, 512> down = {};
    std::array<bool, 512> pressed = {};
    std::array<bool, 512> previous = {};
    int frame = -1;
};

inline LegacyWin32KeyState& GetLegacyWin32KeyState() {
    static LegacyWin32KeyState state;
    return state;
}

inline void RefreshLegacyWin32KeyState() {
    LegacyWin32KeyState& state = GetLegacyWin32KeyState();
    const int frame = GetFrameCount();
    if (state.frame == frame) {
        return;
    }

    state.frame = frame;
    for (int vk = 0; vk < 512; ++vk) {
        const bool is_down = (::GetAsyncKeyState(vk) & 0x8000) != 0;
        state.down[vk] = is_down;
        state.pressed[vk] = is_down && !state.previous[vk];
        state.previous[vk] = is_down;
    }
}

inline bool IsKeyPressed(int key, bool repeat) {
    if (key < 0 || key >= 512) {
        return false;
    }

    if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) {
        return false;
    }

    RefreshLegacyWin32KeyState();
    const LegacyWin32KeyState& state = GetLegacyWin32KeyState();
    if (repeat) {
        return state.down[key];
    }
    return state.pressed[key];
}

inline bool IsKeyDown(int key) {
    if (key < 0 || key >= 512) {
        return false;
    }

    if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) {
        return false;
    }

    RefreshLegacyWin32KeyState();
    return GetLegacyWin32KeyState().down[key];
}

}  // namespace ImGui

#include <imHotKey.h>

namespace PY4GW::imgui::addons::hotkey_demo {

void Render() {
    static std::array<ImHotKey::HotKey, 4> hotkeys = {{
        {"Open Inventory", "Sample action", 0xFFFF172D},
        {"Toggle Overlay", "Sample action", 0xFFFFFF3B},
        {"Center Camera", "Sample action", 0xFFFF211D},
        {"Quick Ping", "Sample action", 0xFFFF1938},
    }};

    ImGui::TextWrapped("ImHotKey gives you a ready-made modal editor for chorded shortcuts.");
    if (ImGui::Button("Edit Hotkeys")) {
        ImGui::OpenPopup("HotKeys Editor");
    }
    ImHotKey::Edit(hotkeys.data(), hotkeys.size(), "HotKeys Editor");

    ImGui::Separator();
    for (const auto& hotkey : hotkeys) {
        char label[128] = {};
        ImHotKey::GetHotKeyLib(hotkey.functionKeys, label, sizeof(label), hotkey.functionName);
        ImGui::BulletText("%s", label);
    }
}

void EditKeys(const char** names, const char** libs, unsigned int* keys, int count, const char* popup_label) {
    if (count <= 0 || !names || !keys) {
        return;
    }
    std::vector<ImHotKey::HotKey> hk(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        hk[i].functionName = names[i];
        hk[i].functionLib = libs ? libs[i] : "";
        hk[i].functionKeys = keys[i];
    }
    ImHotKey::Edit(hk.data(), hk.size(), popup_label);
    for (int i = 0; i < count; ++i) {
        keys[i] = hk[i].functionKeys;
    }
}

void KeyLib(unsigned int keys, char* buffer, std::size_t buffer_size) {
    ImHotKey::GetHotKeyLib(keys, buffer, buffer_size);
}

}  // namespace PY4GW::imgui::addons::hotkey_demo
