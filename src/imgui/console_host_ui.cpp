#include "base/error_handling.h"

#include "imgui/console_host_ui.h"

#include "imgui/compact_ui.h"
#include "imgui/console_ui.h"

#include <imgui.h>

namespace py4gw::imgui::console_host_ui {

namespace {

bool g_show_console = false;
bool g_show_compact_console = true;
bool g_shutdown_confirmation_pending = false;

void RenderShutdownConfirmationModal(bool* request_shutdown) {
    if (g_shutdown_confirmation_pending) {
        ImGui::OpenPopup("Confirm Py4GW Shutdown");
    }

    if (!ImGui::BeginPopupModal("Confirm Py4GW Shutdown", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Closing the console will shut down Py4GW.");
    ImGui::Spacing();

    if (ImGui::Button("Shutdown", ImVec2(120.0f, 0.0f))) {
        if (request_shutdown) {
            *request_shutdown = true;
        }
        g_shutdown_confirmation_pending = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        g_show_compact_console = true;
        g_shutdown_confirmation_pending = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

}  // namespace

bool Initialize() {
    return true;
}

void Shutdown() {
}

void BeginFrame() {
}

void Render(bool* request_shutdown) {
    const bool had_visible_console = g_show_console || g_show_compact_console;

    if (g_show_console) {
        console_ui::RenderFullConsole(&g_show_console, &g_show_compact_console);
    }
    if (g_show_compact_console) {
        compact_ui::RenderCompactConsole(&g_show_console, &g_show_compact_console);
    }

    const bool has_visible_console = g_show_console || g_show_compact_console;
    if (had_visible_console && !has_visible_console) {
        g_shutdown_confirmation_pending = true;
    }

    RenderShutdownConfirmationModal(request_shutdown);
}

}  // namespace py4gw::imgui::console_host_ui
