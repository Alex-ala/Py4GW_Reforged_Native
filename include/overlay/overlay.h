#pragma once

#include "base/error_handling.h"

#include "GW/common/game_pos.h"

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <tuple>

#include <imgui.h>

// ImGui-based primitive handler migrated from legacy py_overlay.h/.cpp.
// Legacy Point2D/Point3D were replaced by the project's GW::Vec2f/GW::Vec3f
// (user-directed deviation); everything else keeps the legacy surface.
namespace PY4GW::overlay {

/* ---------------- Resolved-symbol surface (module-owned) ---------------- */

// Engine helper converting NDC screen coords into a world-space point on the
// near (unk1 = 0) or far (unk1 = 1) plane. The legacy code scanned for this
// inline on every GetScreenToWorld call; byte patterns now live in
// offsets/overlay.json and resolution happens once.
typedef float(__cdecl* ScreenToWorldPoint_pt)(GW::Vec3f* vec, float screen_x, float screen_y, int unk1);

extern ScreenToWorldPoint_pt g_screen_to_world_point_func;
// Static engine Vec2f holding the current mouse position in NDC space.
extern GW::Vec2f* g_ndc_screen_coords;

// Resolver ownership (bodies in overlay_patterns.cpp). Idempotent; lazily
// invoked from GetScreenToWorld. The map raycast (MapCliQueryIntersection) is
// not duplicated here - the overlay reuses GW::map's resolved symbol.
bool ResolveScreenToWorldPointFunc();
bool ResolveNdcScreenCoords();

/* ---------------- Mouse position globals (legacy parity) ---------------- */

extern GW::Vec3f MouseWorldPos;
extern GW::Vec2f MouseScreenPos;

struct GlobalMouseClass {
    void SetMouseWorldPos(float x, float y, float z);
    GW::Vec3f GetMouseWorldPos();

    void SetMouseCoords(float x, float y);
    GW::Vec2f GetMouseCoords();
};

/* --------------- Overlay: ImGui drawlist primitive handler --------------- */

class Overlay {
public:
    ImDrawList* drawList = nullptr;

    DirectX::XMMATRIX CreateViewMatrix(const DirectX::XMFLOAT3& eye_pos, const DirectX::XMFLOAT3& look_at_pos, const DirectX::XMFLOAT3& up_direction);
    DirectX::XMMATRIX CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane);
    GW::Vec3f GetWorldToScreen(const GW::Vec3f& world_position, const DirectX::XMMATRIX& mat_view, const DirectX::XMMATRIX& mat_proj, float viewport_width, float viewport_height);
    void GetScreenToWorld();

public:
    Overlay() = default;

    void RefreshDrawList();
    GW::Vec2f GetMouseCoords();
    float findZ(float x, float y, uint32_t pz);
    uint32_t FindZPlane(float x, float y, uint32_t zplane);
    GW::Vec2f WorldToScreen(float x, float y, float z);
    GW::Vec3f GetMouseWorldPos();

    // Game <-> World
    GW::Vec2f GamePosToWorldMap(float x, float y);
    GW::Vec2f WorlMapToGamePos(float x, float y);

    // World <-> Screen
    GW::Vec2f WorldMapToScreen(float x, float y);
    GW::Vec2f ScreenToWorldMap(float screen_x, float screen_y);

    // Game <-> Screen (combined)
    GW::Vec2f GameMapToScreen(float x, float y);
    GW::Vec2f ScreenToGameMapPos(float screen_x, float screen_y);

    // NormalizedScreen <-> Screen
    GW::Vec2f NormalizedScreenToScreen(float norm_x, float norm_y);
    GW::Vec2f ScreenToNormalizedScreen(float screen_x, float screen_y);

    // NormalizedScreen <-> World / Game
    GW::Vec2f NormalizedScreenToWorldMap(float norm_x, float norm_y);
    GW::Vec2f NormalizedScreenToGameMap(float norm_x, float norm_y);
    GW::Vec2f GamePosToNormalizedScreen(float x, float y);

    void BeginDraw();
    void BeginDraw(std::string name);
    void BeginDraw(std::string name, float x, float y, float width, float height);
    void EndDraw();
    void DrawLine(GW::Vec2f from, GW::Vec2f to, ImU32 color, float thickness);
    void DrawLine3D(GW::Vec3f from, GW::Vec3f to, ImU32 color, float thickness);
    void DrawTriangle(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, ImU32 color, float thickness = 1.0f);
    void DrawTriangle3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, ImU32 color, float thickness = 1.0f);
    void DrawTriangleFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, ImU32 color);
    void DrawTriangleFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, ImU32 color);
    void DrawQuad(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, ImU32 color, float thickness = 1.0f);
    void DrawQuad3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, ImU32 color, float thickness = 1.0f);
    void DrawQuadFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, ImU32 color);
    void DrawQuadFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, ImU32 color);
    void DrawPoly(GW::Vec2f center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f);
    void DrawPolyFilled(GW::Vec2f center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32);
    void DrawPoly3D(GW::Vec3f center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, float thickness = 1.0f, bool autoZ = true);
    void DrawPolyFilled3D(GW::Vec3f center, float radius, ImU32 color = 0xFFFFFFFF, int numSegments = 32, bool autoZ = true);
    void DrawCubeOutline(GW::Vec3f center, float size, ImU32 color, float thickness);
    void DrawCubeFilled(GW::Vec3f center, float size, ImU32 color);
    void DrawText2D(GW::Vec2f position, std::string text, ImU32 color, bool centered = true, float scale = 1.0f);
    void DrawText3D(GW::Vec3f position3D, std::string text, ImU32 color, bool autoZ = true, bool centered = true, float scale = 1.0f);
    void DrawTexture(const std::string& path, float width = 32.0f, float height = 32.0f);
    void DrawTexture(const std::string& path,
        std::tuple<float, float> size,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> tint,
        std::tuple<int, int, int, int> border_col);
    void DrawTexturedRect(float x, float y, float width, float height, const std::string& texture_path);
    void DrawTexturedRect(std::tuple<float, float> pos,
        std::tuple<float, float> size,
        const std::string& texture_path,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> tint);
    void UpkeepTextures(int timeout = 30);
    bool ImageButton(const std::string& caption, const std::string& file_path, float width = 32.0f, float height = 32.0f, int frame_padding = 0);
    bool ImageButton(const std::string& caption, const std::string& file_path,
        std::tuple<float, float> size,
        std::tuple<float, float> uv0,
        std::tuple<float, float> uv1,
        std::tuple<int, int, int, int> bg_color,
        std::tuple<int, int, int, int> tint_color,
        int frame_padding);

    void DrawTextureInForegound(
        const std::tuple<float, float>& pos,
        const std::tuple<float, float>& size,
        const std::string& texture_path,
        const std::tuple<float, float>& uv0,
        const std::tuple<float, float>& uv1,
        const std::tuple<int, int, int, int>& tint);

    void DrawTextureInDrawlist(
        const std::tuple<float, float>& pos,
        const std::tuple<float, float>& size,
        const std::string& texture_path,
        const std::tuple<float, float>& uv0,
        const std::tuple<float, float>& uv1,
        const std::tuple<int, int, int, int>& tint);

    bool IsMouseClicked(int button);
    GW::Vec2f GetDisplaySize();
    void PushClipRect(float x, float y, float x2, float y2);
    void PopClipRect() { drawList->PopClipRect(); }
};

}  // namespace PY4GW::overlay
