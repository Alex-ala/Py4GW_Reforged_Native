#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "GW/native_ui/native_ui.h"

namespace GW::native_ui {

// Definitions for the batch-1 module-owned resolved symbols.
CreateEncodedText_pt g_create_encoded_text_func = nullptr;
CtlFrameListCreateItem_pt g_ctl_frame_list_create_item_func = nullptr;
FrameNewSubclass_pt g_frame_new_subclass_func = nullptr;
uintptr_t g_ctl_text_proc = 0;
uintptr_t g_ctl_text_button_proc = 0;
uintptr_t g_ctl_text_selectable_proc = 0;
uintptr_t g_ui_ctl_page_proc = 0;
uintptr_t g_ui_ctl_slider_proc = 0;
SetHoverTarget_pt g_set_hover_target_func = nullptr;
ClearFocusFrame_pt g_clear_focus_frame_func = nullptr;
uintptr_t g_plain_container_proc = 0;
uintptr_t g_ctl_frame_list_selectable_proc = 0;
CtlFrameListSelectableGetSelection_pt g_ctl_frame_list_selectable_get_selection_func = nullptr;
uintptr_t g_ctl_btn_proc = 0;
FrameMsgCallBase_pt g_frame_msg_call_base_func = nullptr;

bool ResolveCreateEncodedText() {
    if (g_create_encoded_text_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_create_encoded_text");
    if (!PY4GW::Patterns::Resolve("native_ui.create_encoded_text_func", &g_create_encoded_text_func)) {
        return false;
    }
    // Legacy prologue re-validation: the first 6 bytes MUST be
    // 55 8B EC 51 56 57 (PUSH EBP; MOV EBP,ESP; PUSH ECX; PUSH ESI; PUSH EDI).
    // The wildcarded pattern matches 2 sites in every build; the prologue is
    // the disambiguator.
    const auto* bytes = reinterpret_cast<const uint8_t*>(g_create_encoded_text_func);
    if (bytes[0] != 0x55 || bytes[1] != 0x8B || bytes[2] != 0xEC ||
        bytes[3] != 0x51 || bytes[4] != 0x56 || bytes[5] != 0x57) {
        Logger::Instance().LogError("[native_ui] ResolveCreateEncodedText: prologue validation failed.");
        g_create_encoded_text_func = nullptr;
        return false;
    }
    return true;
}

bool ResolveCtlFrameListCreateItem() {
    if (g_ctl_frame_list_create_item_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_frame_list_create_item");
    return PY4GW::Patterns::Resolve("native_ui.ctl_frame_list_create_item_func", &g_ctl_frame_list_create_item_func);
}

bool ResolveFrameNewSubclass() {
    if (g_frame_new_subclass_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_frame_new_subclass");
    return PY4GW::Patterns::Resolve("native_ui.frame_new_subclass_func", &g_frame_new_subclass_func);
}

bool ResolveCtlTextProc() {
    if (g_ctl_text_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_text_proc");
    return PY4GW::Patterns::Resolve("native_ui.ctl_text_proc_func", &g_ctl_text_proc);
}

bool ResolveCtlTextButtonProc() {
    if (g_ctl_text_button_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_text_button_proc");
    return PY4GW::Patterns::Resolve("native_ui.ctl_text_btn_proc_func", &g_ctl_text_button_proc);
}

bool ResolveCtlTextSelectableProc() {
    if (g_ctl_text_selectable_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_text_selectable_proc");
    return PY4GW::Patterns::Resolve("native_ui.ctl_text_selectable_proc_func", &g_ctl_text_selectable_proc);
}

bool ResolveUiCtlPageProc() {
    if (g_ui_ctl_page_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ui_ctl_page_proc");
    return PY4GW::Patterns::Resolve("native_ui.ui_ctl_page_proc_func", &g_ui_ctl_page_proc);
}

bool ResolveUiCtlSliderProc() {
    if (g_ui_ctl_slider_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ui_ctl_slider_proc");
    return PY4GW::Patterns::Resolve("native_ui.ui_ctl_slider_proc_func", &g_ui_ctl_slider_proc);
}

bool ResolveSetHoverTarget() {
    if (g_set_hover_target_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_set_hover_target");
    return PY4GW::Patterns::Resolve("native_ui.set_hover_target_func", &g_set_hover_target_func);
}

bool ResolveClearFocusFrame() {
    if (g_clear_focus_frame_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_clear_focus_frame");
    return PY4GW::Patterns::Resolve("native_ui.clear_focus_frame_func", &g_clear_focus_frame_func);
}

bool ResolvePlainContainerProc() {
    if (g_plain_container_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_plain_container_proc");
    return PY4GW::Patterns::Resolve("native_ui.plain_container_proc_func", &g_plain_container_proc);
}

bool ResolveCtlFrameListSelectableProc() {
    if (g_ctl_frame_list_selectable_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_frame_list_selectable_proc");
    return PY4GW::Patterns::Resolve("native_ui.ctl_frame_list_selectable_proc_func", &g_ctl_frame_list_selectable_proc);
}

bool ResolveCtlFrameListSelectableGetSelection() {
    if (g_ctl_frame_list_selectable_get_selection_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_frame_list_selectable_get_selection");
    return PY4GW::Patterns::Resolve("native_ui.ctl_frame_list_selectable_get_selection_func", &g_ctl_frame_list_selectable_get_selection_func);
}

bool ResolveCtlBtnProc() {
    if (g_ctl_btn_proc) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_ctl_btn_proc");
    return PY4GW::Patterns::Resolve("native_ui.ctl_btn_proc_func", &g_ctl_btn_proc);
}

bool ResolveFrameMsgCallBase() {
    if (g_frame_msg_call_base_func) {
        return true;
    }
    CrashContextScope context("runtime", "native_ui", "resolve_frame_msg_call_base");
    return PY4GW::Patterns::Resolve("native_ui.frame_msg_call_base_func", &g_frame_msg_call_base_func);
}

}  // namespace GW::native_ui
