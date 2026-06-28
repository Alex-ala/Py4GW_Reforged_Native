#include "base/error_handling.h"

#include "imgui/addons/filebrowser_demo.h"

#include <windows.h>

#include <imgui.h>
#include <imfilebrowser.h>

#include <cstring>
#include <filesystem>
#include <memory>

namespace PY4GW::imgui::addons::filebrowser_demo {

namespace {

std::filesystem::path GetSafeInitialDirectory() {
    return std::filesystem::path(L"C:\\");
}

}  // namespace

void Render() {
    static std::unique_ptr<ImGui::FileBrowser> browser;
    static bool init_attempted = false;
    static bool init_failed = false;
    static char selected_path[512] = "No file selected";
    static char error_text[512] = "";

    if (!init_attempted) {
        init_attempted = true;
        try {
            browser = std::make_unique<ImGui::FileBrowser>(
                ImGuiFileBrowserFlags_NoModal |
                ImGuiFileBrowserFlags_CreateNewDir |
                ImGuiFileBrowserFlags_SkipItemsCausingError,
                GetSafeInitialDirectory());
            browser->SetTitle("Addon File Browser");
            browser->SetTypeFilters({".txt", ".json", ".py", ".dll"});
        } catch (const std::exception& error) {
            init_failed = true;
            strncpy_s(error_text, sizeof(error_text), error.what(), _TRUNCATE);
        } catch (...) {
            init_failed = true;
            strncpy_s(error_text, sizeof(error_text), "unknown file browser initialization error", _TRUNCATE);
        }
    }

    ImGui::TextWrapped("Header-only file picker. This wrapper tracks configuration and selection state.");
    if (init_failed || !browser) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "File browser init failed: %s", error_text);
        return;
    }

    if (ImGui::Button("Open File Browser")) {
        browser->Open();
    }

    try {
        browser->Display();
    } catch (const std::exception& error) {
        strncpy_s(error_text, sizeof(error_text), error.what(), _TRUNCATE);
        init_failed = true;
        browser.reset();
    } catch (...) {
        strncpy_s(error_text, sizeof(error_text), "unknown file browser runtime error", _TRUNCATE);
        init_failed = true;
        browser.reset();
    }

    if (init_failed || !browser) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "File browser runtime failed: %s", error_text);
        return;
    }

    if (browser->HasSelected()) {
        const auto selected = browser->GetSelected().string();
        strncpy_s(selected_path, sizeof(selected_path), selected.c_str(), _TRUNCATE);
        browser->ClearSelected();
    }

    ImGui::Separator();
    ImGui::Text("Selected: %s", selected_path);
}

}  // namespace PY4GW::imgui::addons::filebrowser_demo
