#include "base/error_handling.h"

#include "imgui/addons/markdown_demo.h"

#include <windows.h>
#include <shellapi.h>

#include <imgui.h>
#include <imgui_markdown.h>

#include <cstring>
#include <string>

namespace {

void LinkCallback(ImGui::MarkdownLinkCallbackData data) {
    if (data.isImage) {
        return;
    }

    const std::string url(data.link, data.linkLength);
    ::ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData) {
    ImGui::MarkdownImageData image_data;
    image_data.isValid = true;
    image_data.useLinkCallback = false;
    image_data.user_texture_id = ImGui::GetIO().Fonts->TexRef.GetTexID();
    image_data.size = ImVec2(96.0f, 24.0f);
    return image_data;
}

}  // namespace

namespace PY4GW::imgui::addons::markdown_demo {

void Render() {
    static const char* kMarkdown = R"(# imgui_markdown
This demo keeps the integration thin and explicit.

## What is wired
  * Links open with the default shell
  * Images reuse the font atlas as a placeholder texture
  * Formatting stays close to the addon defaults

Visit [Dear ImGui](https://github.com/ocornut/imgui) or [ImPlot](https://github.com/epezent/implot).
)";
    static ImGui::MarkdownConfig config = [] {
        ImGui::MarkdownConfig value;
        value.linkCallback = &LinkCallback;
        value.imageCallback = &ImageCallback;
        value.formatFlags = ImGuiMarkdownFormatFlags_GithubStyle;
        return value;
    }();

    ImGui::TextWrapped("Header-only markdown renderer. This file is the tracking point for callbacks and config.");
    ImGui::Separator();
    ImGui::Markdown(kMarkdown, strlen(kMarkdown), config);
}

void RenderText(const char* text, std::size_t len) {
    static ImGui::MarkdownConfig config = [] {
        ImGui::MarkdownConfig value;
        value.linkCallback = &LinkCallback;
        value.imageCallback = &ImageCallback;
        value.formatFlags = ImGuiMarkdownFormatFlags_GithubStyle;
        return value;
    }();
    if (text && len) {
        ImGui::Markdown(text, len, config);
    }
}

}  // namespace PY4GW::imgui::addons::markdown_demo
