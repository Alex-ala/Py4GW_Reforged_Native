#include "base/error_handling.h"

#include "GW/camera/camera.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

namespace {

bool ResolveCameraPointer() {
    CrashContextScope context("startup", "camera", "resolve_camera_pointer");
    const auto* anchor_pattern = PY4GW::Patterns::Get("camera.camera_ptr_anchor");
    const auto* ptr_scan_pattern = PY4GW::Patterns::Get("camera.camera_ptr_scan");
    if (!anchor_pattern || !ptr_scan_pattern) {
        Logger::Instance().LogError("Missing or invalid camera pointer pattern.", "camera");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        anchor_pattern->assertion_file.c_str(),
        anchor_pattern->assertion_message.c_str(),
        static_cast<uint32_t>(anchor_pattern->line_number),
        anchor_pattern->offset);
    if (!address) {
        Logger::Instance().LogError("Failed to resolve camera assertion anchor.", "camera");
        return false;
    }

    address = PY4GW::Scanner::FindInRange(
        ptr_scan_pattern->pattern.c_str(),
        ptr_scan_pattern->mask.c_str(),
        ptr_scan_pattern->offset,
        address,
        address + anchor_pattern->range);
    if (!Logger::AssertAddress("camera_ptr_scan", address, "camera")) {
        return false;
    }

    const uintptr_t candidate = *reinterpret_cast<const uintptr_t*>(address);
    if (!Logger::AssertAddress("camera_ptr", candidate, "camera")) {
        return false;
    }
    if (!PY4GW::Scanner::IsValidPtr(candidate)) {
        Logger::Instance().LogError("Camera pointer is outside the expected data section.", "camera");
        return false;
    }

    GW::camera::g_camera = reinterpret_cast<GW::Context::Camera*>(candidate);
    return true;
}

bool ResolveFogPatch() {
    CrashContextScope context("startup", "camera", "resolve_fog_patch");
    const auto* pattern = PY4GW::Patterns::Get("camera.fog_patch");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: camera.fog_patch", "camera");
        return false;
    }

    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("camera_fog_patch", address, "camera")) {
        return false;
    }

    static constexpr uint8_t kFogPatch[] = {0x00};
    GW::camera::g_patch_fog.SetPatch(address, kFogPatch, sizeof(kFogPatch));
    return Logger::AssertAddress("camera_fog_patch_target", GW::camera::g_patch_fog.GetAddress(), "camera");
}

bool ResolveCameraUpdatePatch() {
    CrashContextScope context("startup", "camera", "resolve_camera_update_patch");
    const auto* vs2017_pattern = PY4GW::Patterns::Get("camera.camera_update_patch_vs2017");
    const auto* vs2022_pattern = PY4GW::Patterns::Get("camera.camera_update_patch_vs2022");
    if (!vs2017_pattern || !vs2022_pattern) {
        Logger::Instance().LogError("Missing or invalid camera update patch pattern.", "camera");
        return false;
    }

    uintptr_t address = PY4GW::Scanner::Find(
        vs2017_pattern->pattern.c_str(),
        vs2017_pattern->mask.c_str(),
        vs2017_pattern->offset,
        vs2017_pattern->section);
    if (address) {
        static constexpr uint8_t kPatchVs2017[] = {0xEB, 0x0C};
        GW::camera::g_patch_cam_update.SetPatch(address, kPatchVs2017, sizeof(kPatchVs2017));
        return Logger::AssertAddress("camera_update_patch", GW::camera::g_patch_cam_update.GetAddress(), "camera");
    }

    address = PY4GW::Scanner::Find(
        vs2022_pattern->pattern.c_str(),
        vs2022_pattern->mask.c_str(),
        vs2022_pattern->offset,
        vs2022_pattern->section);
    if (!Logger::AssertAddress("camera_update_patch", address, "camera")) {
        return false;
    }

    static constexpr uint8_t kPatchVs2022[] = {0xEB, 0x0F};
    GW::camera::g_patch_cam_update.SetPatch(address, kPatchVs2022, sizeof(kPatchVs2022));
    return Logger::AssertAddress("camera_update_patch_target", GW::camera::g_patch_cam_update.GetAddress(), "camera");
}

void Exit() {
    CrashContextScope context("shutdown", "camera", "exit");
    GW::camera::g_patch_cam_update.Reset();
    GW::camera::g_patch_fog.Reset();
    GW::camera::g_camera = nullptr;
}

}  // namespace

namespace GW::camera {

bool Initialize() {
    CrashContextScope context("startup", "camera", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    if (!ResolveCameraPointer() || !ResolveFogPatch() || !ResolveCameraUpdatePatch()) {
        Exit();
        return false;
    }

    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "camera", "shutdown");
    if (!g_initialized) {
        return;
    }

    Exit();
    g_initialized = false;
}

}  // namespace GW::camera
