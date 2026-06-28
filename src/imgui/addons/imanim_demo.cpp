#include "base/error_handling.h"

#include "imgui/addons/imanim_demo.h"

#include <imgui.h>
#include <im_anim.h>

namespace PY4GW::imgui::addons::imanim_demo {

void BeginFrame() {
    iam_update_begin_frame();
}

void Render() {
    ImGuiIO& io = ImGui::GetIO();
    static bool animate_to_full = false;

    if (ImGui::Button("Toggle Target")) {
        animate_to_full = !animate_to_full;
    }

    const float target = animate_to_full ? 1.0f : 0.2f;
    const ImGuiID id = ImGui::GetID("imanim_progress");
    const float value = iam_tween_float(
        id,
        0,
        target,
        0.45f,
        iam_ease_preset(iam_ease_out_cubic),
        iam_policy_crossfade,
        io.DeltaTime,
        0.2f);

    ImGui::Separator();
    ImGui::Text("Animated value: %.3f", value);
    ImGui::ProgressBar(value, ImVec2(-1.0f, 0.0f));
    ImGui::TextWrapped("ImAnim is compiled in as a real addon source, not just a header include.");
}

}  // namespace PY4GW::imgui::addons::imanim_demo
