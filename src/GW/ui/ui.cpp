#include "base/error_handling.h"

#include "GW/ui/ui.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <shellapi.h>

namespace {

using namespace GW;

bool WaitForUiHooksToDrain() {
    CrashContextScope context("shutdown", "ui", "wait_for_hooks_to_drain");
    for (int i = 0; i < 125; ++i) {
        if (ui::g_active_hooks.load() == 0) {
            return true;
        }
        ::Sleep(16);
    }

    Logger::Instance().LogWarning("[ui] Timed out waiting for in-flight UI hooks to drain.", "ui");
    return false;
}

uintptr_t FindPatternAddress(const char* name) {
    const auto* pattern = PY4GW::Patterns::Get(name);
    if (!pattern) {
        Logger::Instance().LogError(std::string("Missing or invalid pattern: ") + name, "ui");
        return 0;
    }

    if (!pattern->assertion_file.empty() || !pattern->assertion_message.empty()) {
        return PY4GW::Scanner::FindAssertion(
            pattern->assertion_file.c_str(),
            pattern->assertion_message.c_str(),
            static_cast<uint32_t>(pattern->line_number),
            pattern->offset);
    }

    return PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
}

bool ResolveFrameArray() {
    CrashContextScope context("startup", "ui", "resolve_frame_array");
    const uintptr_t address = FindPatternAddress("ui.frame_array_anchor");
    if (!Logger::AssertAddress("s_FrameArray_Ref", address, "ui")) {
        return false;
    }
    ui::g_frame_array = *reinterpret_cast<GW::GWArray<ui::Frame*>**>(address);
    return Logger::AssertAddress("s_FrameArray", reinterpret_cast<uintptr_t>(ui::g_frame_array), "ui");
}

bool ResolveWorldMapState() {
    CrashContextScope context("startup", "ui", "resolve_world_map_state");
    const uintptr_t address = FindPatternAddress("ui.world_map_state");
    if (!Logger::AssertAddress("WorldMapState_Ref", address, "ui")) {
        return false;
    }
    const uintptr_t candidate = *reinterpret_cast<uintptr_t*>(address);
    if (!Logger::AssertAddress("WorldMapState_Addr", candidate, "ui")) {
        return false;
    }
    ui::g_world_map_state_addr = candidate;
    return true;
}

bool ResolveSendFrameUiMessage() {
    CrashContextScope context("startup", "ui", "resolve_send_frame_ui_message");
    const uintptr_t address = FindPatternAddress("ui.send_frame_ui_message_by_id");
    if (!Logger::AssertAddress("SendFrameUIMessageById_Func", address, "ui")) {
        return false;
    }
    ui::g_send_frame_ui_message_by_id_func = reinterpret_cast<ui::SendFrameUIMessageByIdFn>(address);
    ui::g_send_frame_ui_message_func = reinterpret_cast<ui::SendFrameUIMessageFn>(
        PY4GW::Scanner::FunctionFromNearCall(address + 0x67));
    return Logger::AssertAddress("SendFrameUIMessage_Func", reinterpret_cast<uintptr_t>(ui::g_send_frame_ui_message_func), "ui");
}

bool ResolveCreateHashFromWchar() {
    CrashContextScope context("startup", "ui", "resolve_create_hash_from_wchar");
    const uintptr_t address = FindPatternAddress("ui.create_hash_from_wchar");
    if (!Logger::AssertAddress("CreateHashFromWchar_Callsite", address, "ui")) {
        return false;
    }
    ui::g_create_hash_from_wchar_func = reinterpret_cast<ui::CreateHashFromWcharFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("CreateHashFromWchar_Func", reinterpret_cast<uintptr_t>(ui::g_create_hash_from_wchar_func), "ui");
}

bool ResolveGetChildFrameId() {
    CrashContextScope context("startup", "ui", "resolve_get_child_frame_id");
    const uintptr_t address = FindPatternAddress("ui.get_child_frame_id_anchor");
    if (!Logger::AssertAddress("GetChildFrameId_Callsite", address, "ui")) {
        return false;
    }
    ui::g_get_child_frame_id_func = reinterpret_cast<ui::GetChildFrameIdFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("GetChildFrameId_Func", reinterpret_cast<uintptr_t>(ui::g_get_child_frame_id_func), "ui");
}

bool ResolveFindRelatedFrame() {
    CrashContextScope context("startup", "ui", "resolve_find_related_frame");
    const uintptr_t address = FindPatternAddress("ui.find_related_frame");
    if (!Logger::AssertAddress("FindRelatedFrame_Func", address, "ui")) {
        return false;
    }
    ui::g_find_related_frame_func = reinterpret_cast<ui::FindRelatedFrameFn>(address);
    return true;
}

bool ResolveGetRootFrame() {
    CrashContextScope context("startup", "ui", "resolve_get_root_frame");
    const uintptr_t address = FindPatternAddress("ui.get_root_frame");
    ui::g_get_root_frame_func = reinterpret_cast<ui::GetRootFrameFn>(address);
    return Logger::AssertAddress("GetRootFrame_Func", reinterpret_cast<uintptr_t>(ui::g_get_root_frame_func), "ui");
}

bool ResolveSendUiMessage() {
    CrashContextScope context("startup", "ui", "resolve_send_ui_message");
    const uintptr_t address = FindPatternAddress("ui.send_ui_message");
    if (!Logger::AssertAddress("SendUIMessage_Scan", address, "ui")) {
        return false;
    }
    ui::g_send_ui_message_func = reinterpret_cast<ui::SendUIMessageFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("SendUIMessage_Func", reinterpret_cast<uintptr_t>(ui::g_send_ui_message_func), "ui");
}

bool ResolveLoadSettings() {
    CrashContextScope context("startup", "ui", "resolve_load_settings");
    const uintptr_t address = FindPatternAddress("ui.load_settings");
    if (!Logger::AssertAddress("LoadSettings_Callsite", address, "ui")) {
        return false;
    }
    ui::g_load_settings_func = reinterpret_cast<ui::LoadSettingsFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("LoadSettings_Func", reinterpret_cast<uintptr_t>(ui::g_load_settings_func), "ui");
}

bool ResolveUiDrawn() {
    CrashContextScope context("startup", "ui", "resolve_ui_drawn");
    const uintptr_t address = FindPatternAddress("ui.ui_drawn_anchor");
    if (!Logger::AssertAddress("ui_drawn_ref", address, "ui")) {
        return false;
    }
    ui::g_ui_drawn_addr = *reinterpret_cast<uintptr_t*>(address) - 0x10;
    return Logger::AssertAddress("ui_drawn_addr", ui::g_ui_drawn_addr, "ui");
}

bool ResolveShiftScreenshot() {
    CrashContextScope context("startup", "ui", "resolve_shift_screenshot");
    const uintptr_t address = FindPatternAddress("ui.shift_screenshot");
    if (!Logger::AssertAddress("shift_screen_ref", address, "ui")) {
        return false;
    }
    const uintptr_t candidate = *reinterpret_cast<uintptr_t*>(address);
    if (!Logger::AssertAddress("shift_screen_addr", candidate, "ui")) {
        return false;
    }
    ui::g_shift_screen_addr = candidate;
    return true;
}

bool ResolveSetTooltip() {
    CrashContextScope context("startup", "ui", "resolve_set_tooltip");
    const uintptr_t address = FindPatternAddress("ui.set_tooltip");
    if (!Logger::AssertAddress("SetTooltip_Func", address, "ui")) {
        return false;
    }
    const uintptr_t ptr_ref = PY4GW::Scanner::ToFunctionStart(address) + 0x9;
    if (!Logger::AssertAddress("CurrentTooltipPtr_Ref", ptr_ref, "ui")) {
        return false;
    }
    ui::g_current_tooltip_ptr = reinterpret_cast<ui::TooltipInfo***>(*reinterpret_cast<uintptr_t*>(ptr_ref));
    return Logger::AssertAddress("CurrentTooltipPtr", reinterpret_cast<uintptr_t>(ui::g_current_tooltip_ptr), "ui");
}

bool ResolveGameSettings() {
    CrashContextScope context("startup", "ui", "resolve_game_settings");
    const uintptr_t address = FindPatternAddress("ui.game_settings_addr");
    if (!Logger::AssertAddress("GameSettings_Ref", address, "ui")) {
        return false;
    }
    ui::g_game_settings_addr = *reinterpret_cast<uintptr_t*>(address);
    return Logger::AssertAddress("GameSettings_Addr", ui::g_game_settings_addr, "ui");
}

bool ResolveWindowHelpers() {
    CrashContextScope context("startup", "ui", "resolve_window_helpers");
    const uintptr_t address = FindPatternAddress("ui.set_window_visible");
    if (!Logger::AssertAddress("SetWindowVisible_Func", address, "ui")) {
        return false;
    }
    const uintptr_t func = PY4GW::Scanner::ToFunctionStart(address);
    ui::g_set_window_visible_func = reinterpret_cast<ui::SetWindowVisibleFn>(func);
    ui::g_set_window_position_func = reinterpret_cast<ui::SetWindowPositionFn>(func - 0xE0);
    const uintptr_t array_ref = func + 0x49;
    if (!Logger::AssertAddress("window_positions_ref", array_ref, "ui")) {
        return false;
    }
    ui::g_window_positions_array = *reinterpret_cast<ui::WindowPosition**>(array_ref);
    const bool visible_ok = Logger::AssertAddress("SetWindowPosition_Func", reinterpret_cast<uintptr_t>(ui::g_set_window_position_func), "ui");
    const bool array_ok = Logger::AssertAddress("window_positions_array", reinterpret_cast<uintptr_t>(ui::g_window_positions_array), "ui");
    return visible_ok && array_ok;
}

bool ResolveValidateAsyncDecode() {
    CrashContextScope context("startup", "ui", "resolve_async_decode");
    const uintptr_t address = FindPatternAddress("ui.validate_async_decode");
    ui::g_validate_async_decode_str_func = reinterpret_cast<ui::ValidateAsyncDecodeStrFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("ValidateAsyncDecodeStr", reinterpret_cast<uintptr_t>(ui::g_validate_async_decode_str_func), "ui");
}

bool ResolveTitleHelpers() {
    CrashContextScope context("startup", "ui", "resolve_title_helpers");

    uintptr_t get_title_addr = PY4GW::Scanner::FindAssertion(
        "FrNonclient.cpp",
        "ptr->title.Count()",
        0,
        -0x26);
    if (get_title_addr) {
        get_title_addr = PY4GW::Scanner::ToFunctionStart(get_title_addr, 0xFF);
    }
    if (!Logger::AssertAddress("GetTitle_Func", get_title_addr, "ui")) {
        return false;
    }
    ui::g_get_title_func = reinterpret_cast<ui::GetTitleFn>(get_title_addr);

    for (uintptr_t scan = get_title_addr; scan < get_title_addr + 0x100; ++scan) {
        if (*reinterpret_cast<const uint8_t*>(scan) != 0xB9) {
            continue;
        }

        ui::g_title_table_addr = *reinterpret_cast<const uintptr_t*>(scan + 1);
        if (!(ui::g_title_table_addr &&
            PY4GW::Scanner::IsValidPtr(ui::g_title_table_addr, PY4GW::ScannerSection::Data))) {
            ui::g_title_table_addr = 0;
            break;
        }

        for (uintptr_t callsite = scan + 5; callsite < get_title_addr + 0x100; ++callsite) {
            if (*reinterpret_cast<const uint8_t*>(callsite) != 0xE8) {
                continue;
            }

            const uintptr_t candidate = PY4GW::Scanner::FunctionFromNearCall(callsite, true);
            if (candidate) {
                ui::g_title_binary_search_func = reinterpret_cast<ui::TitleBinarySearchFn>(candidate);
                break;
            }
        }
        break;
    }

    const bool title_table_ok = Logger::AssertAddress("TitleTable_Addr", ui::g_title_table_addr, "ui");
    const bool title_search_ok = Logger::AssertAddress("TitleBinarySearch_Func", reinterpret_cast<uintptr_t>(ui::g_title_binary_search_func), "ui");
    return title_table_ok && title_search_ok;
}

bool ResolveDrawOnCompass() {
    CrashContextScope context("startup", "ui", "resolve_draw_on_compass");
    const uintptr_t address = FindPatternAddress("ui.draw_on_compass");
    ui::g_draw_on_compass_func = reinterpret_cast<ui::DrawOnCompassFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("DrawOnCompass_Func", reinterpret_cast<uintptr_t>(ui::g_draw_on_compass_func), "ui");
}

bool ResolveCreateUiComponent() {
    CrashContextScope context("startup", "ui", "resolve_create_ui_component");
    const uintptr_t create_address = FindPatternAddress("ui.create_ui_component");
    if (!Logger::AssertAddress("CreateUIComponent_Scan", create_address, "ui")) {
        return false;
    }
    ui::g_create_ui_component_func = reinterpret_cast<ui::CreateUIComponentFn>(PY4GW::Scanner::ToFunctionStart(create_address));
    const uintptr_t destroy_address = FindPatternAddress("ui.destroy_ui_component");
    if (!Logger::AssertAddress("DestroyUIComponent_Scan", destroy_address, "ui")) {
        return false;
    }
    ui::g_destroy_ui_component_func = reinterpret_cast<ui::DestroyUIComponentFn>(PY4GW::Scanner::ToFunctionStart(destroy_address));
    const bool create_ok = Logger::AssertAddress("CreateUIComponent_Func", reinterpret_cast<uintptr_t>(ui::g_create_ui_component_func), "ui");
    const bool destroy_ok = Logger::AssertAddress("DestroyUIComponent_Func", reinterpret_cast<uintptr_t>(ui::g_destroy_ui_component_func), "ui");
    return create_ok && destroy_ok;
}

bool ResolveFrameNewSubclass() {
    CrashContextScope context("startup", "ui", "resolve_frame_new_subclass");

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        "\\Code\\Engine\\Frame\\FrApi.cpp",
        "frameId",
        0x467,
        0);
    if (address) {
        ui::g_frame_new_subclass_func = reinterpret_cast<ui::FrameNewSubclassFn>(PY4GW::Scanner::ToFunctionStart(address, 0x100));
    }
    if (!ui::g_frame_new_subclass_func) {
        address = PY4GW::Scanner::Find(
            "\x8D\xB8\xA8\x00\x00\x00\x8B\xCF",
            "xxxxxxxx",
            -0x2D);
        if (address) {
            ui::g_frame_new_subclass_func = reinterpret_cast<ui::FrameNewSubclassFn>(address);
        }
    }

    return Logger::AssertAddress("FrameNewSubclass_Func", reinterpret_cast<uintptr_t>(ui::g_frame_new_subclass_func), "ui");
}

bool ResolveTypedComponentPassthrough() {
    CrashContextScope context("startup", "ui", "resolve_typed_component_passthrough");
    const uintptr_t address = FindPatternAddress("ui.typed_component_passthrough");
    if (!Logger::AssertAddress("TypedComponentPassthrough_Scan", address, "ui")) {
        return false;
    }
    ui::g_typed_component_passthrough_func = reinterpret_cast<ui::TypedComponentPassthroughFn>(PY4GW::Scanner::ToFunctionStart(address, 0xFFF));
    return Logger::AssertAddress("TypedComponentPassthrough_Func", reinterpret_cast<uintptr_t>(ui::g_typed_component_passthrough_func), "ui");
}

bool ResolvePreferenceReaders() {
    CrashContextScope context("startup", "ui", "resolve_preference_readers");
    const uintptr_t pref_init = FindPatternAddress("ui.preferences_initialized");
    if (!Logger::AssertAddress("PreferencesInitialised_Ref", pref_init, "ui")) {
        return false;
    }
    ui::g_preferences_initialized_addr = *reinterpret_cast<const uintptr_t*>(pref_init);
    const bool pref_init_ok = Logger::AssertAddress("PreferencesInitialised_Addr", ui::g_preferences_initialized_addr, "ui");

    ui::g_get_string_preference_func = reinterpret_cast<ui::GetStringPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_STRINGS", 0)));
    ui::g_get_flag_preference_func = reinterpret_cast<ui::GetFlagPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_FLAGS", 0)));
    ui::g_get_enum_preference_func = reinterpret_cast<ui::GetEnumPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_ENUMS", 0)));
    ui::g_get_number_preference_func = reinterpret_cast<ui::GetNumberPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_VALUES", 0)));

    const bool get_string_ok = Logger::AssertAddress("GetStringPreference_Func", reinterpret_cast<uintptr_t>(ui::g_get_string_preference_func), "ui");
    const bool get_flag_ok = Logger::AssertAddress("GetFlagPreference_Func", reinterpret_cast<uintptr_t>(ui::g_get_flag_preference_func), "ui");
    const bool get_enum_ok = Logger::AssertAddress("GetEnumPreference_Func", reinterpret_cast<uintptr_t>(ui::g_get_enum_preference_func), "ui");
    const bool get_number_ok = Logger::AssertAddress("GetNumberPreference_Func", reinterpret_cast<uintptr_t>(ui::g_get_number_preference_func), "ui");

    const uintptr_t enum_info = FindPatternAddress("ui.enum_preference_info");
    const uintptr_t value_info = FindPatternAddress("ui.number_preference_info");
    if (!Logger::AssertAddress("EnumPreferenceOptions_Ref", enum_info, "ui") ||
        !Logger::AssertAddress("NumberPreferenceOptions_Ref", value_info, "ui")) {
        return false;
    }
    ui::g_enum_preference_options_addr = *reinterpret_cast<ui::EnumPreferenceInfo**>(enum_info);
    ui::g_number_preference_options_addr = *reinterpret_cast<ui::NumberPreferenceInfo**>(value_info);
    const bool enum_info_ok = Logger::AssertAddress("EnumPreferenceOptions_Addr", reinterpret_cast<uintptr_t>(ui::g_enum_preference_options_addr), "ui");
    const bool value_info_ok = Logger::AssertAddress("NumberPreferenceOptions_Addr", reinterpret_cast<uintptr_t>(ui::g_number_preference_options_addr), "ui");

    return pref_init_ok && get_string_ok && get_flag_ok && get_enum_ok && get_number_ok && enum_info_ok && value_info_ok;
}

bool ResolvePreferenceWriters() {
    CrashContextScope context("startup", "ui", "resolve_preference_writers");

    const uintptr_t set_string_anchor = FindPatternAddress("ui.set_string_preference");
    if (!Logger::AssertAddress("SetStringPreference_Anchor", set_string_anchor, "ui")) {
        return false;
    }
    ui::g_set_string_preference_func = reinterpret_cast<ui::SetStringPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(set_string_anchor));

    const uintptr_t quality_anchor = FindPatternAddress("ui.preference_quality_anchor");
    if (!Logger::AssertAddress("PreferenceQuality_Anchor", quality_anchor, "ui")) {
        return false;
    }
    ui::g_set_enum_preference_func = reinterpret_cast<ui::SetEnumPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x8D));
    ui::g_set_flag_preference_func = reinterpret_cast<ui::SetFlagPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x3B));
    ui::g_set_number_preference_func = reinterpret_cast<ui::SetNumberPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x6A));
    ui::g_set_in_game_static_preference_func = reinterpret_cast<ui::SetInGameStaticPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0xFF));
    ui::g_trigger_terrain_rerender_func = reinterpret_cast<ui::TriggerTerrainRerenderFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x36));

    const uintptr_t shadow_anchor = FindPatternAddress("ui.set_in_game_shadow_quality");
    if (!Logger::AssertAddress("SetInGameShadowQuality_Anchor", shadow_anchor, "ui")) {
        return false;
    }
    ui::g_set_in_game_shadow_quality_func = reinterpret_cast<ui::SetInGameShadowQualityFn>(PY4GW::Scanner::ToFunctionStart(shadow_anchor));

    const uintptr_t ui_scale_anchor = FindPatternAddress("ui.set_in_game_ui_scale");
    if (!Logger::AssertAddress("SetInGameUIScale_Anchor", ui_scale_anchor, "ui")) {
        return false;
    }
    ui::g_set_in_game_ui_scale_func = reinterpret_cast<ui::SetInGameUIScaleFn>(PY4GW::Scanner::FunctionFromNearCall(ui_scale_anchor));

    const uintptr_t volume_anchor = FindPatternAddress("ui.set_volume");
    if (!Logger::AssertAddress("SetVolume_Anchor", volume_anchor, "ui")) {
        return false;
    }
    ui::g_set_volume_func = reinterpret_cast<ui::SetVolumeFn>(PY4GW::Scanner::ToFunctionStart(volume_anchor));

    const uintptr_t master_volume_anchor = FindPatternAddress("ui.set_master_volume");
    if (!Logger::AssertAddress("SetMasterVolume_Anchor", master_volume_anchor, "ui")) {
        return false;
    }
    ui::g_set_master_volume_func = reinterpret_cast<ui::SetMasterVolumeFn>(PY4GW::Scanner::ToFunctionStart(master_volume_anchor));

    const uintptr_t get_renderer_anchor = FindPatternAddress("ui.get_graphics_renderer_value");
    if (!Logger::AssertAddress("GetGraphicsRendererValue_Anchor", get_renderer_anchor, "ui")) {
        return false;
    }
    ui::g_get_graphics_renderer_value_func = reinterpret_cast<ui::GetGraphicsRendererValueFn>(PY4GW::Scanner::FunctionFromNearCall(get_renderer_anchor));

    const uintptr_t set_renderer_anchor = FindPatternAddress("ui.set_graphics_renderer_value");
    if (!Logger::AssertAddress("SetGraphicsRendererValue_Anchor", set_renderer_anchor, "ui")) {
        return false;
    }
    ui::g_set_graphics_renderer_value_func = reinterpret_cast<ui::SetGraphicsRendererValueFn>(PY4GW::Scanner::ToFunctionStart(set_renderer_anchor));

    const uintptr_t set_game_renderer_mode_anchor = FindPatternAddress("ui.set_game_renderer_mode");
    if (!Logger::AssertAddress("SetGameRendererMode_Anchor", set_game_renderer_mode_anchor, "ui")) {
        return false;
    }
    ui::g_set_game_renderer_mode_func = reinterpret_cast<ui::SetGameRendererModeFn>(PY4GW::Scanner::FunctionFromNearCall(set_game_renderer_mode_anchor));

    const uintptr_t game_renderer_metrics_anchor = FindPatternAddress("ui.game_renderer_metrics");
    if (!Logger::AssertAddress("GameRendererMetrics_Anchor", game_renderer_metrics_anchor, "ui")) {
        return false;
    }
    ui::g_get_game_renderer_mode_func = reinterpret_cast<ui::GetGameRendererModeFn>(PY4GW::Scanner::FunctionFromNearCall(game_renderer_metrics_anchor - 0x1D));
    ui::g_get_game_renderer_metric_func = reinterpret_cast<ui::GetGameRendererMetricFn>(PY4GW::Scanner::FunctionFromNearCall(game_renderer_metrics_anchor - 0x5));

    const uintptr_t command_line_number_anchor = FindPatternAddress("ui.command_line_number");
    if (!Logger::AssertAddress("CommandLineNumber_Anchor", command_line_number_anchor, "ui")) {
        return false;
    }
    ui::g_command_line_number_buffer = *reinterpret_cast<uint32_t**>(command_line_number_anchor + 0x29);
    if (ui::g_command_line_number_buffer) {
        ui::g_command_line_number_buffer += 0x30;
    }

    const bool set_string_ok = Logger::AssertAddress("SetStringPreference_Func", reinterpret_cast<uintptr_t>(ui::g_set_string_preference_func), "ui");
    const bool set_enum_ok = Logger::AssertAddress("SetEnumPreference_Func", reinterpret_cast<uintptr_t>(ui::g_set_enum_preference_func), "ui");
    const bool set_flag_ok = Logger::AssertAddress("SetFlagPreference_Func", reinterpret_cast<uintptr_t>(ui::g_set_flag_preference_func), "ui");
    const bool set_number_ok = Logger::AssertAddress("SetNumberPreference_Func", reinterpret_cast<uintptr_t>(ui::g_set_number_preference_func), "ui");
    const bool static_pref_ok = Logger::AssertAddress("SetInGameStaticPreference_Func", reinterpret_cast<uintptr_t>(ui::g_set_in_game_static_preference_func), "ui");
    const bool terrain_ok = Logger::AssertAddress("TriggerTerrainRerender_Func", reinterpret_cast<uintptr_t>(ui::g_trigger_terrain_rerender_func), "ui");
    const bool shadow_ok = Logger::AssertAddress("SetInGameShadowQuality_Func", reinterpret_cast<uintptr_t>(ui::g_set_in_game_shadow_quality_func), "ui");
    const bool ui_scale_ok = Logger::AssertAddress("SetInGameUIScale_Func", reinterpret_cast<uintptr_t>(ui::g_set_in_game_ui_scale_func), "ui");
    const bool volume_ok = Logger::AssertAddress("SetVolume_Func", reinterpret_cast<uintptr_t>(ui::g_set_volume_func), "ui");
    const bool master_volume_ok = Logger::AssertAddress("SetMasterVolume_Func", reinterpret_cast<uintptr_t>(ui::g_set_master_volume_func), "ui");
    const bool get_renderer_ok = Logger::AssertAddress("GetGraphicsRendererValue_Func", reinterpret_cast<uintptr_t>(ui::g_get_graphics_renderer_value_func), "ui");
    const bool set_renderer_ok = Logger::AssertAddress("SetGraphicsRendererValue_Func", reinterpret_cast<uintptr_t>(ui::g_set_graphics_renderer_value_func), "ui");
    const bool set_game_mode_ok = Logger::AssertAddress("SetGameRendererMode_Func", reinterpret_cast<uintptr_t>(ui::g_set_game_renderer_mode_func), "ui");
    const bool get_game_mode_ok = Logger::AssertAddress("GetGameRendererMode_Func", reinterpret_cast<uintptr_t>(ui::g_get_game_renderer_mode_func), "ui");
    const bool get_metric_ok = Logger::AssertAddress("GetGameRendererMetric_Func", reinterpret_cast<uintptr_t>(ui::g_get_game_renderer_metric_func), "ui");
    const bool command_line_ok = Logger::AssertAddress("CommandLineNumber_Buffer", reinterpret_cast<uintptr_t>(ui::g_command_line_number_buffer), "ui");

    return set_string_ok && set_enum_ok && set_flag_ok && set_number_ok &&
        static_pref_ok && terrain_ok && shadow_ok && ui_scale_ok &&
        volume_ok && master_volume_ok && get_renderer_ok && set_renderer_ok &&
        set_game_mode_ok && get_game_mode_ok && get_metric_ok && command_line_ok;
}

bool TryResolveTypedComponentCallbacks() {
    CrashContextScope context("runtime", "ui", "resolve_typed_component_callbacks");
    if (ui::g_typed_component_callbacks_initialized) {
        return true;
    }

    const uintptr_t button_addr = FindPatternAddress("ui.button_frame_callback");
    if (button_addr) {
        ui::g_button_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(button_addr, 0xFF));
    }

    const uintptr_t ctl_btn_addr = FindPatternAddress("ui.ctl_button_proc_callback");
    if (ctl_btn_addr) {
        ui::g_ctl_button_proc_callback = reinterpret_cast<ui::UIInteractionCallback>(ctl_btn_addr);
    }

    const uintptr_t text_btn_addr = FindPatternAddress("ui.text_button_frame_callback");
    if (text_btn_addr) {
        ui::g_text_button_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(text_btn_addr, 0x20));
    }

    const uintptr_t text_label_addr = FindPatternAddress("ui.text_label_frame_callback");
    if (text_label_addr) {
        ui::g_text_label_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(text_label_addr, 0xFFF));
    }

    const uintptr_t scrollable_addr = FindPatternAddress("ui.scrollable_frame_callback");
    if (scrollable_addr) {
        ui::g_scrollable_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(*reinterpret_cast<const uintptr_t*>(scrollable_addr));
    }

    const uintptr_t frame_list_addr = FindPatternAddress("ui.frame_list_callback");
    if (frame_list_addr) {
        ui::g_frame_list_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(frame_list_addr, 0xFFF));
    }

    const uintptr_t dropdown_addr = FindPatternAddress("ui.dropdown_frame_callback");
    if (dropdown_addr) {
        ui::g_dropdown_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(dropdown_addr, 0xFFF));
    }

    const uintptr_t slider_addr = FindPatternAddress("ui.slider_frame_callback");
    if (slider_addr) {
        ui::g_slider_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(slider_addr, 0xFFF));
    }

    const uintptr_t slider_wrapper_addr = FindPatternAddress("ui.slider_frame_wrapper_callback");
    if (slider_wrapper_addr) {
        ui::g_slider_frame_wrapper_callback = reinterpret_cast<ui::UIInteractionCallback>(slider_wrapper_addr);
    }

    const uintptr_t editable_addr = FindPatternAddress("ui.editable_text_frame_callback");
    if (editable_addr) {
        ui::g_editable_text_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(editable_addr, 0xFFF));
    }

    const uintptr_t progress_addr = FindPatternAddress("ui.progress_bar_callback");
    if (progress_addr) {
        ui::g_progress_bar_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(progress_addr, 0xFFF));
    }

    const uintptr_t tabs_addr = FindPatternAddress("ui.tabs_frame_callback");
    if (tabs_addr) {
        ui::g_tabs_frame_callback = reinterpret_cast<ui::UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(tabs_addr, 0xFFF));
    }

    ui::g_typed_component_callbacks_initialized = true;
    return true;
}

void __cdecl OnSendUIMessage(ui::UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++ui::g_active_hooks;
    if (!ui::g_shutting_down) {
        ui::SendUIMessage(message_id, wparam, lparam);
    } else if (ui::g_send_ui_message_original) {
        ui::g_send_ui_message_original(message_id, wparam, lparam);
    }
    --ui::g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

void OnOpenTemplateUiMessage(PY4GW::HookStatus* hook_status, ui::UIMessage msgid, void* wparam, void*) {
    PY4GW_ASSERT(msgid == ui::UIMessage::kOpenTemplate && wparam);
    auto* info = static_cast<ui::ChatTemplate*>(wparam);
    if (!(ui::g_open_links && info && info->code.valid() && info->name)) {
        return;
    }
    if (!wcsncmp(info->name, L"http://", 7) || !wcsncmp(info->name, L"https://", 8)) {
        hook_status->blocked = true;
        ::ShellExecuteW(nullptr, L"open", info->name, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void __cdecl OnSendFrameUIMessageById(uint32_t frame_id, ui::UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++ui::g_active_hooks;
    if (!ui::g_shutting_down) {
        ui::Frame* frame = ui::GetFrameById(frame_id);
        if (frame) {
            ui::SendFrameUIMessage(frame, message_id, wparam, lparam);
        }
    } else if (ui::g_send_frame_ui_message_by_id_original) {
        ui::g_send_frame_ui_message_by_id_original(frame_id, message_id, wparam, lparam);
    }
    --ui::g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

void __fastcall OnSendFrameUIMessage(GW::GWArray<ui::UIInteractionCallback>* frame_callbacks, void*, ui::UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++ui::g_active_hooks;
    if (!ui::g_shutting_down && frame_callbacks) {
        auto* frame = reinterpret_cast<ui::Frame*>(reinterpret_cast<uintptr_t>(frame_callbacks) - 0xA8);
        ui::SendFrameUIMessage(frame, message_id, wparam, lparam);
    } else if (ui::g_send_frame_ui_message_original) {
        ui::g_send_frame_ui_message_original(frame_callbacks, nullptr, message_id, wparam, lparam);
    }
    --ui::g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

uint32_t __cdecl OnCreateUIComponent(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label) {
    PY4GW::HookBase::EnterHook();
    ++ui::g_active_hooks;

    uint32_t result = 0;
    if (ui::g_shutting_down || !ui::g_create_ui_component_original) {
        if (ui::g_create_ui_component_original) {
            result = ui::g_create_ui_component_original(frame_id, component_flags, tab_index, event_callback, name_enc, component_label);
        }
    } else {
        ui::CreateUIComponentPacket packet{
            frame_id,
            component_flags,
            tab_index,
            reinterpret_cast<ui::UIInteractionCallback>(event_callback),
            name_enc,
            component_label};

        std::vector<ui::CreateUIComponentCallbackEntry> callbacks;
        if (ui::g_callback_mutex_initialized) {
            ::EnterCriticalSection(&ui::g_callback_mutex);
            callbacks = ui::g_create_ui_component_callbacks;
            ::LeaveCriticalSection(&ui::g_callback_mutex);
        }

        PY4GW::HookStatus status;
        size_t i = 0;
        for (; i < callbacks.size(); ++i) {
            if (callbacks[i].altitude > 0) {
                break;
            }
            callbacks[i].callback(&packet);
            ++status.altitude;
        }

        result = ui::g_create_ui_component_original(
            packet.frame_id,
            packet.component_flags,
            packet.tab_index,
            reinterpret_cast<void*>(packet.event_callback),
            packet.name_enc,
            packet.component_label);

        for (; i < callbacks.size(); ++i) {
            callbacks[i].callback(&packet);
            ++status.altitude;
        }
    }

    --ui::g_active_hooks;
    PY4GW::HookBase::LeaveHook();
    return result;
}

bool Init() {
    CrashContextScope context("startup", "ui", "init");
    ::InitializeCriticalSection(&ui::g_callback_mutex);
    ui::g_callback_mutex_initialized = true;

    if (!ResolveFrameArray() ||
        !ResolveWorldMapState() ||
        !ResolveSendFrameUiMessage() ||
        !ResolveCreateHashFromWchar() ||
        !ResolveGetChildFrameId() ||
        !ResolveFindRelatedFrame() ||
        !ResolveGetRootFrame() ||
        !ResolveSendUiMessage() ||
        !ResolveLoadSettings() ||
        !ResolveUiDrawn() ||
        !ResolveShiftScreenshot() ||
        !ResolveSetTooltip() ||
        !ResolveGameSettings() ||
        !ResolveWindowHelpers() ||
        !ResolveValidateAsyncDecode() ||
        !ResolveTitleHelpers() ||
        !ResolveDrawOnCompass() ||
        !ResolveCreateUiComponent() ||
        !ResolveFrameNewSubclass() ||
        !ResolveTypedComponentPassthrough() ||
        !ResolvePreferenceReaders() ||
        !ResolvePreferenceWriters()) {
        return false;
    }

    const bool send_ui_ok = Logger::AssertHook(
        "SendUIMessage_Func",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&ui::g_send_ui_message_func),
            reinterpret_cast<void*>(&OnSendUIMessage),
            reinterpret_cast<void**>(&ui::g_send_ui_message_original)),
        "ui");
    const bool send_frame_by_id_ok = Logger::AssertHook(
        "SendFrameUIMessageById_Func",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&ui::g_send_frame_ui_message_by_id_func),
            reinterpret_cast<void*>(&OnSendFrameUIMessageById),
            reinterpret_cast<void**>(&ui::g_send_frame_ui_message_by_id_original)),
        "ui");
    const bool send_frame_ok = Logger::AssertHook(
        "SendFrameUIMessage_Func",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&ui::g_send_frame_ui_message_func),
            reinterpret_cast<void*>(&OnSendFrameUIMessage),
            reinterpret_cast<void**>(&ui::g_send_frame_ui_message_original)),
        "ui");
    const bool create_component_ok = Logger::AssertHook(
        "CreateUIComponent_Func",
        PY4GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&ui::g_create_ui_component_func),
            reinterpret_cast<void*>(&OnCreateUIComponent),
            reinterpret_cast<void**>(&ui::g_create_ui_component_original)),
        "ui");
    return send_ui_ok && send_frame_by_id_ok && send_frame_ok && create_component_ok;
}

void EnableHooks() {
    CrashContextScope context("runtime", "ui", "enable_hooks");
    if (ui::g_send_ui_message_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(ui::g_send_ui_message_func));
    }
    if (ui::g_send_frame_ui_message_by_id_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(ui::g_send_frame_ui_message_by_id_func));
    }
    if (ui::g_send_frame_ui_message_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(ui::g_send_frame_ui_message_func));
    }
    if (ui::g_create_ui_component_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(ui::g_create_ui_component_func));
    }
    ui::RegisterUIMessageCallback(&ui::g_open_template_hook, ui::UIMessage::kOpenTemplate, &OnOpenTemplateUiMessage);
}

void DisableHooks() {
    CrashContextScope context("shutdown", "ui", "disable_hooks");
    ui::RemoveUIMessageCallback(&ui::g_open_template_hook);
    if (ui::g_send_ui_message_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(ui::g_send_ui_message_func));
    }
    if (ui::g_send_frame_ui_message_by_id_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(ui::g_send_frame_ui_message_by_id_func));
    }
    if (ui::g_send_frame_ui_message_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(ui::g_send_frame_ui_message_func));
    }
    if (ui::g_create_ui_component_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(ui::g_create_ui_component_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "ui", "exit");
    if (ui::g_callback_mutex_initialized) {
        ::EnterCriticalSection(&ui::g_callback_mutex);
        ui::g_ui_message_callbacks.clear();
        ui::g_frame_ui_message_callbacks.clear();
        ui::g_create_ui_component_callbacks.clear();
        ::LeaveCriticalSection(&ui::g_callback_mutex);
    }

    if (ui::g_send_ui_message_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(ui::g_send_ui_message_func));
    }
    if (ui::g_send_frame_ui_message_by_id_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(ui::g_send_frame_ui_message_by_id_func));
    }
    if (ui::g_send_frame_ui_message_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(ui::g_send_frame_ui_message_func));
    }
    if (ui::g_create_ui_component_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(ui::g_create_ui_component_func));
    }

    if (ui::g_callback_mutex_initialized) {
        ::DeleteCriticalSection(&ui::g_callback_mutex);
        ui::g_callback_mutex_initialized = false;
    }

    ui::g_send_ui_message_func = nullptr;
    ui::g_send_ui_message_original = nullptr;
    ui::g_send_frame_ui_message_func = nullptr;
    ui::g_send_frame_ui_message_original = nullptr;
    ui::g_send_frame_ui_message_by_id_func = nullptr;
    ui::g_send_frame_ui_message_by_id_original = nullptr;
    ui::g_create_hash_from_wchar_func = nullptr;
    ui::g_get_child_frame_id_func = nullptr;
    ui::g_find_related_frame_func = nullptr;
    ui::g_get_root_frame_func = nullptr;
    ui::g_load_settings_func = nullptr;
    ui::g_set_window_visible_func = nullptr;
    ui::g_set_window_position_func = nullptr;
    ui::g_validate_async_decode_str_func = nullptr;
    ui::g_title_binary_search_func = nullptr;
    ui::g_get_title_func = nullptr;
    ui::g_draw_on_compass_func = nullptr;
    ui::g_create_ui_component_func = nullptr;
    ui::g_create_ui_component_original = nullptr;
    ui::g_destroy_ui_component_func = nullptr;
    ui::g_frame_new_subclass_func = nullptr;
    ui::g_typed_component_passthrough_func = nullptr;
    ui::g_get_flag_preference_func = nullptr;
    ui::g_set_flag_preference_func = nullptr;
    ui::g_get_string_preference_func = nullptr;
    ui::g_set_string_preference_func = nullptr;
    ui::g_get_enum_preference_func = nullptr;
    ui::g_set_enum_preference_func = nullptr;
    ui::g_get_number_preference_func = nullptr;
    ui::g_set_number_preference_func = nullptr;
    ui::g_get_graphics_renderer_value_func = nullptr;
    ui::g_set_graphics_renderer_value_func = nullptr;
    ui::g_get_game_renderer_mode_func = nullptr;
    ui::g_set_game_renderer_mode_func = nullptr;
    ui::g_get_game_renderer_metric_func = nullptr;
    ui::g_set_in_game_shadow_quality_func = nullptr;
    ui::g_set_in_game_static_preference_func = nullptr;
    ui::g_trigger_terrain_rerender_func = nullptr;
    ui::g_set_in_game_ui_scale_func = nullptr;
    ui::g_set_volume_func = nullptr;
    ui::g_set_master_volume_func = nullptr;
    ui::g_enum_preference_options_addr = nullptr;
    ui::g_number_preference_options_addr = nullptr;
    ui::g_command_line_number_buffer = nullptr;
    ui::g_button_frame_callback = nullptr;
    ui::g_ctl_button_proc_callback = nullptr;
    ui::g_text_button_frame_callback = nullptr;
    ui::g_scrollable_frame_callback = nullptr;
    ui::g_text_label_frame_callback = nullptr;
    ui::g_frame_list_callback = nullptr;
    ui::g_dropdown_frame_callback = nullptr;
    ui::g_slider_frame_callback = nullptr;
    ui::g_slider_frame_wrapper_callback = nullptr;
    ui::g_editable_text_frame_callback = nullptr;
    ui::g_progress_bar_callback = nullptr;
    ui::g_tabs_frame_callback = nullptr;
    ui::g_typed_component_callbacks_initialized = false;
    ui::g_frame_array = nullptr;
    ui::g_world_map_state_addr = 0;
    ui::g_preferences_initialized_addr = 0;
    ui::g_title_table_addr = 0;
    ui::g_ui_drawn_addr = 0;
    ui::g_shift_screen_addr = 0;
    ui::g_game_settings_addr = 0;
    ui::g_current_tooltip_ptr = nullptr;
    ui::g_window_positions_array = nullptr;
    ui::g_open_links = false;
    ui::g_active_hooks = 0;
}

}  // namespace

namespace GW::ui {

SendUIMessageFn g_send_ui_message_func = nullptr;
SendUIMessageFn g_send_ui_message_original = nullptr;
SendFrameUIMessageFn g_send_frame_ui_message_func = nullptr;
SendFrameUIMessageFn g_send_frame_ui_message_original = nullptr;
SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_func = nullptr;
SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_original = nullptr;
CreateHashFromWcharFn g_create_hash_from_wchar_func = nullptr;
GetChildFrameIdFn g_get_child_frame_id_func = nullptr;
FindRelatedFrameFn g_find_related_frame_func = nullptr;
GetRootFrameFn g_get_root_frame_func = nullptr;
LoadSettingsFn g_load_settings_func = nullptr;
SetWindowVisibleFn g_set_window_visible_func = nullptr;
SetWindowPositionFn g_set_window_position_func = nullptr;
ValidateAsyncDecodeStrFn g_validate_async_decode_str_func = nullptr;
TitleBinarySearchFn g_title_binary_search_func = nullptr;
GetTitleFn g_get_title_func = nullptr;
DrawOnCompassFn g_draw_on_compass_func = nullptr;
CreateUIComponentFn g_create_ui_component_func = nullptr;
CreateUIComponentFn g_create_ui_component_original = nullptr;
DestroyUIComponentFn g_destroy_ui_component_func = nullptr;
FrameNewSubclassFn g_frame_new_subclass_func = nullptr;
TypedComponentPassthroughFn g_typed_component_passthrough_func = nullptr;
GetFlagPreferenceFn g_get_flag_preference_func = nullptr;
SetFlagPreferenceFn g_set_flag_preference_func = nullptr;
GetStringPreferenceFn g_get_string_preference_func = nullptr;
SetStringPreferenceFn g_set_string_preference_func = nullptr;
GetEnumPreferenceFn g_get_enum_preference_func = nullptr;
SetEnumPreferenceFn g_set_enum_preference_func = nullptr;
GetNumberPreferenceFn g_get_number_preference_func = nullptr;
SetNumberPreferenceFn g_set_number_preference_func = nullptr;
GetGraphicsRendererValueFn g_get_graphics_renderer_value_func = nullptr;
SetGraphicsRendererValueFn g_set_graphics_renderer_value_func = nullptr;
GetGameRendererModeFn g_get_game_renderer_mode_func = nullptr;
SetGameRendererModeFn g_set_game_renderer_mode_func = nullptr;
GetGameRendererMetricFn g_get_game_renderer_metric_func = nullptr;
SetInGameShadowQualityFn g_set_in_game_shadow_quality_func = nullptr;
SetInGameStaticPreferenceFn g_set_in_game_static_preference_func = nullptr;
TriggerTerrainRerenderFn g_trigger_terrain_rerender_func = nullptr;
SetInGameUIScaleFn g_set_in_game_ui_scale_func = nullptr;
SetVolumeFn g_set_volume_func = nullptr;
SetMasterVolumeFn g_set_master_volume_func = nullptr;
EnumPreferenceInfo* g_enum_preference_options_addr = nullptr;
NumberPreferenceInfo* g_number_preference_options_addr = nullptr;
uint32_t* g_command_line_number_buffer = nullptr;
GW::GWArray<Frame*>* g_frame_array = nullptr;
uintptr_t g_world_map_state_addr = 0;
uintptr_t g_preferences_initialized_addr = 0;
uintptr_t g_title_table_addr = 0;
uintptr_t g_ui_drawn_addr = 0;
uintptr_t g_shift_screen_addr = 0;
uintptr_t g_game_settings_addr = 0;
TooltipInfo*** g_current_tooltip_ptr = nullptr;
WindowPosition* g_window_positions_array = nullptr;
CRITICAL_SECTION g_callback_mutex;
bool g_callback_mutex_initialized = false;
std::unordered_map<UIMessage, std::vector<UIMessageCallbackEntry>> g_ui_message_callbacks;
std::unordered_map<UIMessage, std::vector<FrameUIMessageCallbackEntry>> g_frame_ui_message_callbacks;
std::vector<CreateUIComponentCallbackEntry> g_create_ui_component_callbacks;
bool g_open_links = false;
PY4GW::HookEntry g_open_template_hook;
std::atomic<bool> g_initialized = false;
std::atomic<bool> g_shutting_down = false;
std::atomic<uint32_t> g_active_hooks = 0;
uint32_t g_create_flat_button_dialog_subclass_type = 0;
UIInteractionCallback g_button_frame_callback = nullptr;
UIInteractionCallback g_ctl_button_proc_callback = nullptr;
UIInteractionCallback g_text_button_frame_callback = nullptr;
UIInteractionCallback g_scrollable_frame_callback = nullptr;
UIInteractionCallback g_text_label_frame_callback = nullptr;
UIInteractionCallback g_frame_list_callback = nullptr;
UIInteractionCallback g_dropdown_frame_callback = nullptr;
UIInteractionCallback g_slider_frame_callback = nullptr;
UIInteractionCallback g_slider_frame_wrapper_callback = nullptr;
UIInteractionCallback g_editable_text_frame_callback = nullptr;
UIInteractionCallback g_progress_bar_callback = nullptr;
UIInteractionCallback g_tabs_frame_callback = nullptr;
bool g_typed_component_callbacks_initialized = false;

bool InitializeTypedComponentCallbacks() {
    return TryResolveTypedComponentCallbacks();
}

bool Initialize() {
    CrashContextScope context("startup", "ui", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    PY4GW::HookBase::Initialize();
    if (!Init()) {
        Exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    EnableHooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "ui", "shutdown");
    if (!g_initialized) {
        return;
    }

    g_shutting_down = true;
    DisableHooks();
    WaitForUiHooksToDrain();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_shutting_down = false;
    g_initialized = false;
}

}  // namespace GW::ui
