#pragma once

#include "base/error_handling.h"
#include "base/memory_patcher.h"

#include <atomic>
#include <cstdint>

namespace gw::camera {

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

constexpr Vec3f operator-(Vec3f lhs, const Vec3f& rhs) {
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    return lhs;
}

struct Camera {
    uint32_t look_at_agent_id;
    uint32_t h0004;
    float h0008;
    float h000C;
    float max_distance;
    float h0014;
    float yaw;
    float pitch;
    float distance;
    uint32_t h0024[4];
    float yaw_right_click;
    float yaw_right_click2;
    float pitch_right_click;
    float distance2;
    float acceleration_constant;
    float time_since_last_keyboard_rotation;
    float time_since_last_mouse_rotation;
    float time_since_last_mouse_move;
    float time_since_last_agent_selection;
    float time_in_the_map;
    float time_in_the_district;
    float yaw_to_go;
    float pitch_to_go;
    float dist_to_go;
    float max_distance2;
    float h0070[2];
    Vec3f position;
    Vec3f camera_pos_to_go;
    Vec3f cam_pos_inverted;
    Vec3f cam_pos_inverted_to_go;
    Vec3f look_at_target;
    Vec3f look_at_to_go;
    float field_of_view;
    float field_of_view2;
    uint32_t h00C8;
    uint32_t h00CC;
    uint32_t h00D0;
    uint32_t h00D4;
    uint32_t h00D8;
    uint32_t h00DC;
    uint32_t h00E0;
    uint32_t h00E4;
    uint32_t h00E8;
    uint32_t h00EC;
    uint32_t h00F0;
    uint32_t h00F4;
    uint32_t h00F8;
    uint32_t h00FC;
    uint32_t h0100;
    uint32_t h0104;
    uint32_t h0108;
    uint32_t h010C;
    uint32_t h0110;
    uint32_t h0114;
    uint32_t h0118;
    uint32_t camera_mode;

    float GetYaw() const {
        return yaw;
    }

    float GetFieldOfView() const {
        return field_of_view;
    }

    void SetYaw(float value) {
        yaw_to_go = value;
        yaw = value;
    }

    float GetCurrentYaw() const;

    void SetPitch(float value) {
        pitch_to_go = value;
    }

    float GetCameraZoom() const {
        return distance;
    }

    Vec3f GetLookAtTarget() const {
        return look_at_target;
    }

    void SetCameraPos(Vec3f value) {
        position = value;
    }
};

Camera* GetCamera();

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

extern Camera* g_camera;
extern py4gw::MemoryPatcher g_patch_cam_update;
extern py4gw::MemoryPatcher g_patch_fog;
extern std::atomic<bool> g_initialized;

}  // namespace gw::camera
