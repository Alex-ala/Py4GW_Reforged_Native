#pragma once

#include "base/error_handling.h"

#include "GW/context/camera.h"
#include "base/memory_patcher.h"

#include <atomic>
#include <cstdint>

namespace GW::camera {

bool Initialize();
void Shutdown();

Context::Camera* GetCamera();

bool ForwardMovement(float amount, bool true_forward);
bool VerticalMovement(float amount);
bool RotateMovement(float angle);
bool SideMovement(float amount);

bool SetMaxDist(float dist = 900.0f);
bool SetFieldOfView(float fov);
Vec3f ComputeCamPos(float dist = 0.0f);
bool UpdateCameraPos();

float GetFieldOfView();
float GetYaw();

bool UnlockCam(bool flag);
bool GetCameraUnlock();
bool SetFog(bool flag);

extern Context::Camera* g_camera;
extern PY4GW::MemoryPatcher g_patch_cam_update;
extern PY4GW::MemoryPatcher g_patch_fog;
extern std::atomic<bool> g_initialized;

}  // namespace GW::camera
