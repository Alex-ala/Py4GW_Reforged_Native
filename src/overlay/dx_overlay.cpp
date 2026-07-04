#include "base/error_handling.h"

#include "overlay/dx_overlay.h"

#include "GW/context/camera.h"
#include "GW/context/context.h"
#include "GW/context/pathing.h"
#include "GW/map/map.h"
#include "GW/render/render.h"
#include "GW/textures/texture_manager.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <windows.h>
#include <DirectXMath.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <wincodec.h>

using namespace DirectX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

namespace PY4GW::overlay {

namespace {

// UTF-8 -> UTF-16 for TextureManager keys (same helper shape as
// texture_bindings.cpp; the legacy byte-widening loop broke non-ASCII paths).
std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

}  // namespace

void DXOverlay::set_primitives(const std::vector<std::vector<GW::Vec2f>>& prims, D3DCOLOR draw_color) {
    primitives = prims;
    color = draw_color;
}

void DXOverlay::set_world_zoom_x(float zoom) { world_zoom_x = zoom; }
void DXOverlay::set_world_zoom_y(float zoom) { world_zoom_y = zoom; }
void DXOverlay::set_world_pan(float x, float y) { world_pan_x = x; world_pan_y = y; }
void DXOverlay::set_world_rotation(float r) { world_rotation = r; }
void DXOverlay::set_world_space(bool enabled) { world_space = enabled; }
void DXOverlay::set_world_scale(float x) { world_scale = x; }

void DXOverlay::set_screen_offset(float x, float y) { screen_offset_x = x; screen_offset_y = y; }
void DXOverlay::set_screen_zoom_x(float zoom) { screen_zoom_x = zoom; }
void DXOverlay::set_screen_zoom_y(float zoom) { screen_zoom_y = zoom; }
void DXOverlay::set_screen_rotation(float r) { screen_rotation = r; }

void DXOverlay::set_circular_mask(bool enabled) { use_circular_mask = enabled; }
void DXOverlay::set_circular_mask_radius(float radius) { mask_radius = radius; }
void DXOverlay::set_circular_mask_center(float x, float y) { mask_center_x = x; mask_center_y = y; }

void DXOverlay::set_rectangle_mask(bool enabled) { use_rectangle_mask = enabled; }
void DXOverlay::set_rectangle_mask_bounds(float x, float y, float width, float height) {
    mask_rect_x = x;
    mask_rect_y = y;
    mask_rect_width = width;
    mask_rect_height = height;
}

void DXOverlay::SetupProjection() {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    D3DVIEWPORT9 vp;
    device->GetViewport(&vp);

    float w = 5000.0f * 2;

    XMMATRIX ortho(
        2.0f / w, 0, 0, 0,
        0, 2.0f / w, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );

    float xscale = world_scale;
    float yscale = (static_cast<float>(vp.Width) / static_cast<float>(vp.Height)) * world_scale;

    float xtrans = 0.0f; // optional pan offset
    float ytrans = 0.0f; // optional pan offset

    XMMATRIX vp_matrix(
        xscale, 0, 0, 0,
        0, yscale, 0, 0,
        0, 0, 1, 0,
        xtrans, ytrans, 0, 1
    );

    XMMATRIX proj = ortho * vp_matrix;

    device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));
}

void DXOverlay::build_pathing_trapezoid_geometry(D3DCOLOR color) {
    this->color = color;   // SAME behavior as set_primitives

    primitives.clear();

    const auto* pmaps = GW::map::GetPathingMap();
    if (!pmaps || pmaps->size() == 0)
        return;

    // Optional reserve
    size_t total = 0;
    for (size_t i = 0; i < pmaps->size(); ++i) {
        total += (*pmaps)[i].trapezoid_count;
    }
    primitives.reserve(total);

    for (size_t i = 0; i < pmaps->size(); ++i) {
        const GW::Context::PathingMap& layer = (*pmaps)[i];
        const GW::Context::PathingTrapezoid* traps = layer.trapezoids;
        if (!traps || layer.trapezoid_count == 0)
            continue;

        for (uint32_t t = 0; t < layer.trapezoid_count; ++t) {
            const GW::Context::PathingTrapezoid& tr = traps[t];

            std::vector<GW::Vec2f> quad;
            quad.reserve(4);

            quad.emplace_back(tr.XTL, tr.YT);
            quad.emplace_back(tr.XTR, tr.YT);
            quad.emplace_back(tr.XBL, tr.YB);
            quad.emplace_back(tr.XBR, tr.YB);

            primitives.emplace_back(std::move(quad));
        }
    }
}

void DXOverlay::inverse_rendering(bool enabled) {
    inverse_rendering_enabled = enabled;
}

void DXOverlay::ApplyStencilMask() {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    auto FillRect = [device](float x, float y, float w, float h, D3DCOLOR color) {
        D3DVertex vertices[4] = {
            {x,     y,     0.0f, 1.0f, color},
            {x + w, y,     0.0f, 1.0f, color},
            {x,     y + h, 0.0f, 1.0f, color},
            {x + w, y + h, 0.0f, 1.0f, color}
        };
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
        };

    auto FillCircle = [device](float x, float y, float radius, D3DCOLOR color, unsigned resolution = 192u) {
        D3DVertex vertices[193];
        for (unsigned i = 0; i <= resolution; ++i) {
            float angle = static_cast<float>(i) / resolution * XM_2PI;
            vertices[i] = {
                x + radius * cosf(angle),
                y + radius * sinf(angle),
                0.0f,
                1.0f,
                color
            };
        }
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, resolution, vertices, sizeof(D3DVertex));
        };

    device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
    device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0);

    device->SetRenderState(D3DRS_STENCILREF, 1);
    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

    if (use_circular_mask)
        FillCircle(mask_center_x, mask_center_y, mask_radius, D3DCOLOR_ARGB(0, 0, 0, 0));
    else if (use_rectangle_mask)
        FillRect(mask_rect_x, mask_rect_y, mask_rect_width, mask_rect_height, D3DCOLOR_ARGB(0, 0, 0, 0));
    else
        FillRect(-5000.0f, -5000.0f, 10000.0f, 10000.0f, D3DCOLOR_ARGB(0, 0, 0, 0));

    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
}

void DXOverlay::ResetStencilMask() {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    device->SetRenderState(D3DRS_STENCILREF, 0);
    device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
    device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
}

void DXOverlay::render() {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device || primitives.empty()) return;

    IDirect3DStateBlock9* state_block = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state_block))) return;

    D3DMATRIX old_world, old_view, old_proj;
    device->GetTransform(D3DTS_WORLD, &old_world);
    device->GetTransform(D3DTS_VIEW, &old_view);
    device->GetTransform(D3DTS_PROJECTION, &old_proj);

    if (world_space) {
        SetupProjection();

        auto translate = XMMatrixTranslation(-world_pan_x, -world_pan_y, 0);
        auto rotateZ = XMMatrixRotationZ(world_rotation);
        auto zoom = XMMatrixScaling(world_zoom_x, world_zoom_y, 1.0f);
        auto scale = XMMatrixScaling(world_scale, world_scale, 1.0f);
        auto flipY = XMMatrixScaling(1.0f, -1.0f, 1.0f); // flip happens HERE

        auto view = translate * rotateZ * scale * flipY * zoom;

        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));
        device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    }
    else {
        device->SetTransform(D3DTS_VIEW, &old_view);
        device->SetTransform(D3DTS_PROJECTION, &old_proj);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    }

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
    device->SetIndices(nullptr);
    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, false);
    device->SetRenderState(D3DRS_ZENABLE, false);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    auto FillRect = [device](float x, float y, float w, float h, D3DCOLOR color) {
        D3DVertex vertices[4] = {
            {x,     y,     0.0f, 1.0f, color},
            {x + w, y,     0.0f, 1.0f, color},
            {x,     y + h, 0.0f, 1.0f, color},
            {x + w, y + h, 0.0f, 1.0f, color}
        };
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
        };

    auto FillCircle = [device](float x, float y, float radius, D3DCOLOR color, unsigned resolution = 192u) {
        D3DVertex vertices[193];
        for (unsigned i = 0; i <= resolution; ++i) {
            float angle = static_cast<float>(i) / resolution * XM_2PI;
            vertices[i] = {
                x + radius * cosf(angle),
                y + radius * sinf(angle),
                0.0f,
                1.0f,
                color
            };
        }
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, resolution, vertices, sizeof(D3DVertex));
        };

    if (inverse_rendering_enabled) {
        D3DVIEWPORT9 vp;
        device->GetViewport(&vp);

        float cos_r = cosf(world_rotation);
        float sin_r = sinf(world_rotation);

        auto BuildScreenVerts = [&](const std::vector<GW::Vec2f>& shape, D3DVertex* verts) {
            for (size_t i = 0; i < shape.size(); ++i) {
                float x = shape[i].x;
                float y = shape[i].y;

                float x_out = x, y_out = y;
                if (world_space) {
                    x_out = x * cos_r - y * sin_r;
                    y_out = x * sin_r + y * cos_r;

                    x_out *= world_scale;
                    y_out *= world_scale;

                    x_out = x_out * world_zoom_x + world_pan_x + screen_offset_x;
                    y_out = y_out * world_zoom_y + world_pan_y + screen_offset_y;
                }

                verts[i] = { x_out, y_out, 0.0f, 1.0f, color };
            }
        };

        device->SetTexture(0, nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
        device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

        const bool has_mask = use_circular_mask || use_rectangle_mask;
        if (has_mask) {
            device->SetRenderState(D3DRS_STENCILREF, 2);
            if (use_circular_mask)
                FillCircle(mask_center_x, mask_center_y, mask_radius, D3DCOLOR_ARGB(0, 0, 0, 0));
            else
                FillRect(mask_rect_x, mask_rect_y, mask_rect_width, mask_rect_height, D3DCOLOR_ARGB(0, 0, 0, 0));
        }

        device->SetRenderState(D3DRS_STENCILREF, 1);

        for (const auto& shape : primitives) {
            if (shape.size() != 3 && shape.size() != 4) continue;

            D3DVertex verts[4];
            BuildScreenVerts(shape, verts);

            if (shape.size() == 4)
                device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(D3DVertex));
            else
                device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
        }

        device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        device->SetRenderState(D3DRS_STENCILREF, has_mask ? 2 : 1);
        device->SetRenderState(D3DRS_STENCILFUNC, has_mask ? D3DCMP_EQUAL : D3DCMP_NOTEQUAL);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);

        static IDirect3DPixelShader9* inverse_shader = nullptr;
        static IDirect3DDevice9* inverse_shader_device = nullptr;
        if (inverse_shader_device != device) {
            if (inverse_shader) {
                inverse_shader->Release();
                inverse_shader = nullptr;
            }
            inverse_shader_device = device;
        }

        if (!inverse_shader) {
            static const char shader_source[] =
                "sampler2D scene_sampler : register(s0);\n"
                "float4 tint : register(c0);\n"
                "struct PS_INPUT { float2 uv : TEXCOORD0; };\n"
                "float4 main(PS_INPUT input) : COLOR0 {\n"
                "    float4 scene = tex2D(scene_sampler, input.uv);\n"
                "    float3 inverted = tint.rgb * (1.0f - scene.rgb);\n"
                "    return float4(lerp(scene.rgb, inverted, tint.a), scene.a);\n"
                "}\n";

            // Legacy compiled with D3DXCompileShader (retired DirectX SDK);
            // D3DCompile from d3dcompiler is the drop-in modern equivalent.
            ID3DBlob* shader_buffer = nullptr;
            ID3DBlob* error_buffer = nullptr;
            HRESULT shader_hr = D3DCompile(
                shader_source,
                sizeof(shader_source) - 1,
                nullptr,
                nullptr,
                nullptr,
                "main",
                "ps_2_0",
                0,
                0,
                &shader_buffer,
                &error_buffer);

            if (SUCCEEDED(shader_hr) && shader_buffer) {
                shader_hr = device->CreatePixelShader(
                    static_cast<const DWORD*>(shader_buffer->GetBufferPointer()),
                    &inverse_shader);
            }

            if (shader_buffer) shader_buffer->Release();
            if (error_buffer) error_buffer->Release();
        }

        IDirect3DSurface9* current_rt = nullptr;
        IDirect3DTexture9* scene_texture = nullptr;
        IDirect3DSurface9* scene_surface = nullptr;
        HRESULT copy_hr = inverse_shader ? device->GetRenderTarget(0, &current_rt) : E_FAIL;
        D3DSURFACE_DESC rt_desc = {};
        if (SUCCEEDED(copy_hr) && current_rt) {
            copy_hr = current_rt->GetDesc(&rt_desc);
        }
        if (SUCCEEDED(copy_hr)) {
            copy_hr = device->CreateTexture(
                rt_desc.Width,
                rt_desc.Height,
                1,
                D3DUSAGE_RENDERTARGET,
                rt_desc.Format,
                D3DPOOL_DEFAULT,
                &scene_texture,
                nullptr);
        }
        if (SUCCEEDED(copy_hr) && scene_texture) {
            copy_hr = scene_texture->GetSurfaceLevel(0, &scene_surface);
        }
        if (SUCCEEDED(copy_hr) && scene_surface) {
            copy_hr = device->StretchRect(current_rt, nullptr, scene_surface, nullptr, D3DTEXF_NONE);
        }

        if (SUCCEEDED(copy_hr) && scene_texture && inverse_shader) {
            struct InverseVertex {
                float x, y, z, rhw;
                D3DCOLOR color;
                float u, v;
            };

            const float tint[] = {
                static_cast<float>((color >> 16) & 0xFF) / 255.0f,
                static_cast<float>((color >> 8) & 0xFF) / 255.0f,
                static_cast<float>(color & 0xFF) / 255.0f,
                static_cast<float>((color >> 24) & 0xFF) / 255.0f
            };

            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
            device->SetTexture(0, scene_texture);
            device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            device->SetPixelShader(inverse_shader);
            device->SetPixelShaderConstantF(0, tint, 1);

            InverseVertex fullscreen[] = {
                { static_cast<float>(vp.X), static_cast<float>(vp.Y), 0.0f, 1.0f, color, 0.0f, 0.0f },
                { static_cast<float>(vp.X + vp.Width), static_cast<float>(vp.Y), 0.0f, 1.0f, color, 1.0f, 0.0f },
                { static_cast<float>(vp.X), static_cast<float>(vp.Y + vp.Height), 0.0f, 1.0f, color, 0.0f, 1.0f },
                { static_cast<float>(vp.X + vp.Width), static_cast<float>(vp.Y + vp.Height), 0.0f, 1.0f, color, 1.0f, 1.0f }
            };
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, fullscreen, sizeof(InverseVertex));
            device->SetPixelShader(nullptr);
            device->SetTexture(0, nullptr);
        }

        if (scene_surface) scene_surface->Release();
        if (scene_texture) scene_texture->Release();
        if (current_rt) current_rt->Release();

        device->SetTransform(D3DTS_WORLD, &old_world);
        device->SetTransform(D3DTS_VIEW, &old_view);
        device->SetTransform(D3DTS_PROJECTION, &old_proj);

        state_block->Apply();
        state_block->Release();
        return;
    }
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

    if (use_circular_mask) {
        device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
        device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        device->SetRenderState(D3DRS_STENCILREF, 1);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

        FillCircle(mask_center_x, mask_center_y, mask_radius, D3DCOLOR_ARGB(0, 0, 0, 0));

        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    }
    else if (use_rectangle_mask) {
        device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
        device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);
        device->SetRenderState(D3DRS_STENCILREF, 1);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);

        FillRect(mask_rect_x, mask_rect_y, mask_rect_width, mask_rect_height, D3DCOLOR_ARGB(0, 0, 0, 0));

        // Set stencil test to only draw where the rect was filled
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    }
    else {
        FillRect(-5000.0f, -5000.0f, 10000.0f, 10000.0f, D3DCOLOR_ARGB(0, 0, 0, 0));
    }

    float cos_r = cosf(world_rotation);
    float sin_r = sinf(world_rotation);

    for (const auto& shape : primitives) {
        if (shape.size() != 3 && shape.size() != 4) continue;

        D3DVertex verts[4];
        for (size_t i = 0; i < shape.size(); ++i) {
            float x = shape[i].x;
            float y = shape[i].y;

            float x_out = x, y_out = y;
            if (world_space) {
                x_out = x * cos_r - y * sin_r;
                y_out = x * sin_r + y * cos_r;

                x_out *= world_scale;
                y_out *= world_scale;

                x_out = x_out * world_zoom_x + world_pan_x + screen_offset_x;
                y_out = y_out * world_zoom_y + world_pan_y + screen_offset_y;
            }

            verts[i] = {
                x_out, y_out, 0.0f, 1.0f, color
            };
        }

        if (shape.size() == 4)
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(D3DVertex));
        else
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
    }

    if (use_circular_mask || use_rectangle_mask) {
        device->SetRenderState(D3DRS_STENCILREF, 0);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    }

    device->SetTransform(D3DTS_WORLD, &old_world);
    device->SetTransform(D3DTS_VIEW, &old_view);
    device->SetTransform(D3DTS_PROJECTION, &old_proj);

    state_block->Apply();
    state_block->Release();
}

void DXOverlay::DrawLine(GW::Vec2f from, GW::Vec2f to, D3DCOLOR color, float thickness) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    // Ensure 2D screen-space rendering is not affected by Z-buffer
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    if (thickness <= 1.0f) {
        D3DVertex verts[] = {
            { from.x, from.y, 0.0f, 1.0f, color },
            { to.x,   to.y,   0.0f, 1.0f, color }
        };
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(D3DVertex));
        return;
    }

    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0.0f) return;

    float nx = -dy / len;
    float ny = dx / len;
    float half = thickness * 0.5f;

    D3DVertex quad[] = {
        { from.x - nx * half, from.y - ny * half, 0.0f, 1.0f, color },
        { from.x + nx * half, from.y + ny * half, 0.0f, 1.0f, color },
        { to.x - nx * half, to.y - ny * half, 0.0f, 1.0f, color },
        { to.x + nx * half, to.y + ny * half, 0.0f, 1.0f, color }
    };

    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(D3DVertex));
}

void DXOverlay::DrawTriangle(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, D3DCOLOR color, float thickness) {
    DrawLine(p1, p2, color, thickness);
    DrawLine(p2, p3, color, thickness);
    DrawLine(p3, p1, color, thickness);
}

void DXOverlay::DrawTriangleFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, D3DCOLOR color) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    // Ensure 2D screen-space rendering is not affected by Z-buffer
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    D3DVertex verts[] = {
        { p1.x, p1.y, 0.0f, 1.0f, color },
        { p2.x, p2.y, 0.0f, 1.0f, color },
        { p3.x, p3.y, 0.0f, 1.0f, color }
    };
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
}

void DXOverlay::DrawQuad(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, D3DCOLOR color, float thickness) {
    DrawLine(p1, p2, color, thickness);
    DrawLine(p2, p3, color, thickness);
    DrawLine(p3, p4, color, thickness);
    DrawLine(p4, p1, color, thickness);
}

void DXOverlay::DrawQuadFilled(GW::Vec2f p1, GW::Vec2f p2, GW::Vec2f p3, GW::Vec2f p4, D3DCOLOR color) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    // Ensure 2D screen-space rendering is not affected by Z-buffer
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    D3DVertex verts[6] = {
        { p1.x, p1.y, 0.0f, 1.0f, color },
        { p2.x, p2.y, 0.0f, 1.0f, color },
        { p3.x, p3.y, 0.0f, 1.0f, color },
        { p3.x, p3.y, 0.0f, 1.0f, color },
        { p4.x, p4.y, 0.0f, 1.0f, color },
        { p1.x, p1.y, 0.0f, 1.0f, color }
    };

    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(D3DVertex));
}

void DXOverlay::DrawPoly(GW::Vec2f center, float radius, D3DCOLOR color, int segments, float thickness) {
    float step = 2.0f * 3.141592654f / segments;
    for (int i = 0; i < segments; ++i) {
        float a0 = step * i;
        float a1 = step * (i + 1);
        GW::Vec2f p1(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
        GW::Vec2f p2(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
        DrawLine(p1, p2, color, thickness);
    }
}

void DXOverlay::DrawPolyFilled(GW::Vec2f center, float radius, D3DCOLOR color, int segments) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    // Ensure 2D screen-space rendering is not affected by Z-buffer
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    std::vector<D3DVertex> verts;
    verts.emplace_back(D3DVertex{ center.x, center.y, 0.0f, 1.0f, color });

    float step = 2.0f * 3.141592654f / segments;
    for (int i = 0; i <= segments; ++i) {
        float angle = step * i;
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        verts.emplace_back(D3DVertex{ x, y, 0.0f, 1.0f, color });
    }

    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, segments, verts.data(), sizeof(D3DVertex));
}

void DXOverlay::Setup3DView() {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    // Lighting and blending
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFF);

    // Depth testing
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

    auto* cam = GW::Context::GetCamera();
    if (!cam) return;

    // Right-handed camera vectors (flip Z). Legacy used D3DXMatrix* helpers;
    // DirectXMath is the drop-in replacement (same row-major layout).
    XMFLOAT3 eye(cam->position.x, cam->position.y, -cam->position.z);
    XMFLOAT3 at(cam->look_at_target.x, cam->look_at_target.y, -cam->look_at_target.z);
    XMFLOAT3 up(0.0f, 0.0f, 1.0f); // RH: Z+ is up

    XMMATRIX view = XMMatrixLookAtRH(XMLoadFloat3(&eye), XMLoadFloat3(&at), XMLoadFloat3(&up));
    device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));

    float fov = GW::render::GetFieldOfView();
    float aspect = static_cast<float>(GW::render::GetViewportWidth()) / static_cast<float>(GW::render::GetViewportHeight());
    float near_plane = 48000.0f / 1024.0f;
    float far_plane = 48000.0f;

    XMMATRIX proj = XMMatrixPerspectiveFovRH(fov, aspect, near_plane, far_plane);
    device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));

    // Set identity world transform
    XMMATRIX identity = XMMatrixIdentity();
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&identity));
}

void DXOverlay::DrawLine3D(GW::Vec3f from, GW::Vec3f to, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    Setup3DView();

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // Single segment: original behavior
    if (segments <= 1) {
        D3DVertex3D v[2] = {
            { from.x, from.y, -from.z, color },
            { to.x,   to.y,   -to.z,   color }
        };
        device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_LINELIST, 1, v, sizeof(D3DVertex3D));
        return;
    }

    // N segments: snap each segment's endpoints to ground using its own z-plane (midpoint z)
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    for (int i = 0; i < segments; ++i) {
        float t0 = float(i) / float(segments);
        float t1 = float(i + 1) / float(segments);
        float x0 = lerp(from.x, to.x, t0);
        float y0 = lerp(from.y, to.y, t0);
        float x1 = lerp(from.x, to.x, t1);
        float y1 = lerp(from.y, to.y, t1);

        float z0 = overlay.findZ(x0, y0, 0) - floor_offset;
        float z1 = overlay.findZ(x1, y1, 0) - floor_offset;

        D3DVertex3D seg[2] = {
            { x0, y0, -z0, color },
            { x1, y1, -z1, color }
        };
        device->DrawPrimitiveUP(D3DPT_LINELIST, 1, seg, sizeof(D3DVertex3D));
    }
}

// ---------- TRIANGLE (outline) ----------
void DXOverlay::DrawTriangle3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, D3DCOLOR color, bool use_occlusion, int edge_segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    // Reuse the line routine (per-segment FindZ, RH flip, etc.)
    DrawLine3D(p1, p2, color, use_occlusion, edge_segments, floor_offset);
    DrawLine3D(p2, p3, color, use_occlusion, edge_segments, floor_offset);
    DrawLine3D(p3, p1, color, use_occlusion, edge_segments, floor_offset);
}

void DXOverlay::DrawTriangleFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    Setup3DView();
    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto LerpP = [&](const GW::Vec3f& A, const GW::Vec3f& B, float t) {
        return GW::Vec3f{ lerp(A.x,B.x,t), lerp(A.y,B.y,t), lerp(A.z,B.z,t) };
        };

    // Subdivide the triangle into 'segments' radial strips from p1
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    for (int i = 0; i < segments; ++i) {
        float t0 = float(i) / float(segments);
        float t1 = float(i + 1) / float(segments);

        // Two edges from p1: p1->p2 and p1->p3
        GW::Vec3f a0 = LerpP(p1, p2, t0);
        GW::Vec3f a1 = LerpP(p1, p2, t1);
        GW::Vec3f b0 = LerpP(p1, p3, t0);
        GW::Vec3f b1 = LerpP(p1, p3, t1);

        // First small triangle: (a0, b0, b1)
        {
            // Use this triangle's centroid z as z-plane selector
            float zplane = (a0.z + b0.z + b1.z) / 3.0f;
            float za0 = overlay.findZ(a0.x, a0.y, static_cast<uint32_t>(zplane));
            float zb0 = overlay.findZ(b0.x, b0.y, static_cast<uint32_t>(zplane));
            float zb1 = overlay.findZ(b1.x, b1.y, static_cast<uint32_t>(zplane));

            V tri[3] = {
                { a0.x, a0.y, -za0, color },
                { b0.x, b0.y, -zb0, color },
                { b1.x, b1.y, -zb1, color }
            };
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }

        // Second small triangle: (a0, b1, a1)
        {
            float zplane = (a0.z + b1.z + a1.z) / 3.0f;
            float za0 = overlay.findZ(a0.x, a0.y, static_cast<uint32_t>(zplane));
            float zb1 = overlay.findZ(b1.x, b1.y, static_cast<uint32_t>(zplane));
            float za1 = overlay.findZ(a1.x, a1.y, static_cast<uint32_t>(zplane));

            V tri[3] = {
                { a0.x, a0.y, -za0, color },
                { b1.x, b1.y, -zb1, color },
                { a1.x, a1.y, -za1, color }
            };
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }
    }
}

// ---------- QUAD (outline) ----------
void DXOverlay::DrawQuad3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, D3DCOLOR color, bool use_occlusion, int edge_segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    DrawLine3D(p1, p2, color, use_occlusion, edge_segments, floor_offset);
    DrawLine3D(p2, p3, color, use_occlusion, edge_segments, floor_offset);
    DrawLine3D(p3, p4, color, use_occlusion, edge_segments, floor_offset);
    DrawLine3D(p4, p1, color, use_occlusion, edge_segments, floor_offset);
}

void DXOverlay::DrawQuadFilled3D(GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4, D3DCOLOR color, bool use_occlusion, int segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    // Split into two triangles; reuse the draped triangle filler
    DrawTriangleFilled3D(p1, p2, p3, color, use_occlusion, segments, floor_offset);
    DrawTriangleFilled3D(p3, p4, p1, color, use_occlusion, segments, floor_offset);
}

// ---------- POLY (outline) ----------
// 'numSegments' = number of polygon sides (existing meaning).
// 'segments' = per-edge snapping subdivisions.
void DXOverlay::DrawPoly3D(GW::Vec3f center, float radius, D3DCOLOR color, int numSegments, bool autoZ, bool use_occlusion, int segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    Setup3DView();
    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    const float step = XM_2PI / numSegments;

    // Precompute ring points (XY); Z will be sampled per sub-segment
    std::vector<GW::Vec3f> ring;
    ring.reserve(numSegments + 1);
    for (int i = 0; i <= numSegments; ++i) {
        float a = step * i;
        float x = center.x + cosf(a) * radius;
        float y = center.y + sinf(a) * radius;
        float z = autoZ ? overlay.findZ(x, y, static_cast<uint32_t>(center.z)) : center.z; // base Z (not final)
        ring.push_back({ x, y, z });
    }

    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    // For each edge, split into 'segments' and snap each small segment
    for (int i = 0; i < numSegments; ++i) {
        GW::Vec3f A = ring[i];
        GW::Vec3f B = ring[i + 1];

        for (int s = 0; s < std::max(1, segments); ++s) {
            float t0 = float(s) / float(std::max(1, segments));
            float t1 = float(s + 1) / float(std::max(1, segments));
            float x0 = A.x + (B.x - A.x) * t0;
            float y0 = A.y + (B.y - A.y) * t0;
            float x1 = A.x + (B.x - A.x) * t1;
            float y1 = A.y + (B.y - A.y) * t1;

            // Use this sub-segment's midpoint z as the z-plane selector
            float zplane = (A.z + B.z) * 0.5f;
            float z0 = overlay.findZ(x0, y0, static_cast<uint32_t>(zplane)) - floor_offset;
            float z1 = overlay.findZ(x1, y1, static_cast<uint32_t>(zplane)) - floor_offset;

            V seg[2] = {
                { x0, y0, -z0, color },
                { x1, y1, -z1, color }
            };
            device->DrawPrimitiveUP(D3DPT_LINELIST, 1, seg, sizeof(V));
        }
    }
}

// ---------- POLY (filled, draped by triangulating each fan triangle and snapping per sub-tri) ----------
void DXOverlay::DrawPolyFilled3D(GW::Vec3f center, float radius, D3DCOLOR color, int numSegments, bool autoZ, bool use_occlusion, int segments, float floor_offset) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    Setup3DView();
    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    struct V { float x, y, z; D3DCOLOR c; };
    const float step = XM_2PI / numSegments;

    // Precompute center Z (if requested)
    float zc = autoZ ? overlay.findZ(center.x, center.y, static_cast<uint32_t>(center.z)) : center.z - floor_offset;

    // Precompute ring points (XY only; Z is sampled per sub-tri)
    std::vector<GW::Vec3f> ring;
    ring.reserve(numSegments + 1);
    for (int i = 0; i <= numSegments; ++i) {
        float a = step * i;
        ring.push_back({ center.x + cosf(a) * radius,
                         center.y + sinf(a) * radius,
                         center.z });
    }

    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    for (int i = 0; i < numSegments; ++i) {
        GW::Vec3f A = ring[i];
        GW::Vec3f B = ring[i + 1];

        // Split fan triangle (center, A, B) along edge A->B
        int n = std::max(1, segments);
        for (int s = 0; s < n; ++s) {
            float t0 = float(s) / float(n);
            float t1 = float(s + 1) / float(n);

            GW::Vec3f E0{ A.x + (B.x - A.x) * t0, A.y + (B.y - A.y) * t0, center.z };
            GW::Vec3f E1{ A.x + (B.x - A.x) * t1, A.y + (B.y - A.y) * t1, center.z };

            // Triangle (center, E0, E1): pick this tri's centroid z as z-plane
            float zplane = (zc + E0.z + E1.z) / 3.0f;

            float zCenter = overlay.findZ(center.x, center.y, static_cast<uint32_t>(zplane));
            float zE0 = overlay.findZ(E0.x, E0.y, static_cast<uint32_t>(zplane)) - floor_offset;
            float zE1 = overlay.findZ(E1.x, E1.y, static_cast<uint32_t>(zplane)) - floor_offset;

            V tri[3] = {
                { center.x, center.y, -zCenter, color },
                { E0.x,     E0.y,     -zE0,     color },
                { E1.x,     E1.y,     -zE1,     color }
            };
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(V));
        }
    }
}

void DXOverlay::DrawCubeOutline(GW::Vec3f center, float size, D3DCOLOR color, bool use_occlusion) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    Setup3DView();

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    float h = size * 0.5f;

    GW::Vec3f c[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h}
    };

    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (int i = 0; i < 12; ++i) {
        DrawLine3D(c[edges[i][0]], c[edges[i][1]], color, use_occlusion);
    }
}

void DXOverlay::DrawCubeFilled(GW::Vec3f center, float size, D3DCOLOR color, bool use_occlusion) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;
    Setup3DView();

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    float h = size * 0.5f;

    GW::Vec3f c[8] = {
        {center.x - h, center.y - h, center.z - h},
        {center.x + h, center.y - h, center.z - h},
        {center.x + h, center.y + h, center.z - h},
        {center.x - h, center.y + h, center.z - h},
        {center.x - h, center.y - h, center.z + h},
        {center.x + h, center.y - h, center.z + h},
        {center.x + h, center.y + h, center.z + h},
        {center.x - h, center.y + h, center.z + h}
    };

    const int faces[6][4] = {
        {0,1,2,3}, // back
        {4,5,6,7}, // front
        {0,1,5,4}, // bottom
        {2,3,7,6}, // top
        {1,2,6,5}, // right
        {0,3,7,4}  // left
    };

    for (int i = 0; i < 6; ++i) {
        DrawQuadFilled3D(c[faces[i][0]], c[faces[i][1]], c[faces[i][2]], c[faces[i][3]], color, use_occlusion);
    }
}

void DXOverlay::DrawTexture(const std::string& file_path, float screen_pos_x, float screen_pos_y, float width, float height, uint32_t int_tint) {
    // Legacy forced the tint to opaque white here; the port respects int_tint.
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);

    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    LPDIRECT3DTEXTURE9 texture = GW::textures::TextureManager::Instance().GetTexture(Widen(file_path));
    if (!texture) return;

    float w = width * 0.5f;
    float h = height * 0.5f;

    struct Vertex {
        float x, y, z, rhw;
        float u, v;
        D3DCOLOR color;
    };

    Vertex quad[] = {
        { screen_pos_x - w, screen_pos_y - h, 0.0f, 1.0f, 0.0f, 0.0f, tint },
        { screen_pos_x + w, screen_pos_y - h, 0.0f, 1.0f, 1.0f, 0.0f, tint },
        { screen_pos_x + w, screen_pos_y + h, 0.0f, 1.0f, 1.0f, 1.0f, tint },
        { screen_pos_x - w, screen_pos_y + h, 0.0f, 1.0f, 0.0f, 1.0f, tint },
    };

    // Set render states for textures
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    device->SetTexture(0, texture);

    // Set texture blending
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, sizeof(Vertex));
}

void DXOverlay::DrawTexture3D(const std::string& file_path, float world_pos_x, float world_pos_y, float world_pos_z, float width, float height, bool use_occlusion, uint32_t int_tint) {
    // Legacy forced the tint to opaque white here; the port respects int_tint.
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);

    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    LPDIRECT3DTEXTURE9 texture = GW::textures::TextureManager::Instance().GetTexture(Widen(file_path));
    if (!texture) return;

    Setup3DView();

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    // Flip Z to match RH coordinate system
    world_pos_z = -world_pos_z;

    float w = width / 2.0f;
    float h = height / 2.0f;

    struct D3DTexturedVertex3D {
        float x, y, z;
        float u, v;
        D3DCOLOR color;
    };

    D3DTexturedVertex3D verts[] = {
        { world_pos_x - w, world_pos_y - h, world_pos_z, 0.0f, 0.0f, tint },
        { world_pos_x + w, world_pos_y - h, world_pos_z, 1.0f, 0.0f, tint },
        { world_pos_x + w, world_pos_y + h, world_pos_z, 1.0f, 1.0f, tint },
        { world_pos_x - w, world_pos_y + h, world_pos_z, 0.0f, 1.0f, tint }
    };

    device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    device->SetTexture(0, texture);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTexturedVertex3D));

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
}

void DXOverlay::DrawQuadTextured3D(const std::string& file_path,
    GW::Vec3f p1, GW::Vec3f p2, GW::Vec3f p3, GW::Vec3f p4,
    bool use_occlusion,
    uint32_t int_tint) {
    // Legacy forced the tint to opaque white here; the port respects int_tint.
    D3DCOLOR tint = D3DCOLOR_ARGB((int_tint >> 24) & 0xFF, (int_tint >> 16) & 0xFF, (int_tint >> 8) & 0xFF, int_tint & 0xFF);

    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return;

    LPDIRECT3DTEXTURE9 texture = GW::textures::TextureManager::Instance().GetTexture(Widen(file_path));
    if (!texture) return;

    Setup3DView();

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }

    // Flip Z to match RH system
    p1.z = -p1.z;
    p2.z = -p2.z;
    p3.z = -p3.z;
    p4.z = -p4.z;

    struct D3DTexturedVertex3D {
        float x, y, z;
        float u, v;
        D3DCOLOR color;
    };

    D3DTexturedVertex3D verts[] = {
        { p1.x, p1.y, p1.z, 0.0f, 0.0f, tint },
        { p2.x, p2.y, p2.z, 1.0f, 0.0f, tint },
        { p3.x, p3.y, p3.z, 1.0f, 1.0f, tint },
        { p4.x, p4.y, p4.z, 0.0f, 1.0f, tint }
    };

    device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    device->SetTexture(0, texture);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(D3DTexturedVertex3D));

    if (!use_occlusion) {
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
}

enum SaveGeometryResult {
    SAVE_OK = 0,
    ERR_NO_DEVICE,
    ERR_NO_PRIMITIVES,
    ERR_INVALID_DIMENSIONS,
    ERR_CREATE_TEXTURE,
    ERR_GET_SURFACE,
    ERR_GET_OLD_RT,
    ERR_SET_RT,
    ERR_CREATE_STATEBLOCK,
    ERR_SET_VIEWPORT,
    ERR_SAVE_FILE,
    ERR_STATEBLOCK_APPLY,
    ERR_UNKNOWN
};

namespace {

// COM apartment + WIC factory helpers, same shape as texture_manager.cpp.
void EnsureComInitialized() {
    static thread_local bool initialized = false;
    if (initialized) {
        return;
    }
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        initialized = true;
    }
}

IWICImagingFactory* GetWicFactory() {
    static IWICImagingFactory* factory = nullptr;
    if (factory) {
        return factory;
    }
    EnsureComInitialized();
    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    return SUCCEEDED(hr) ? factory : nullptr;
}

// PNG export via WIC; replaces the legacy D3DXSaveSurfaceToFileW (retired
// DirectX SDK). The render target is A8R8G8B8 which maps to 32bppBGRA.
HRESULT SaveSurfaceToPngWIC(IDirect3DDevice9* device, IDirect3DSurface9* rt_surface, const std::wstring& filename) {
    D3DSURFACE_DESC desc{};
    HRESULT hr = rt_surface->GetDesc(&desc);
    if (FAILED(hr)) return hr;

    IDirect3DSurface9* sysmem = nullptr;
    hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sysmem, nullptr);
    if (FAILED(hr)) return hr;

    hr = device->GetRenderTargetData(rt_surface, sysmem);
    if (FAILED(hr)) { sysmem->Release(); return hr; }

    D3DLOCKED_RECT lr{};
    hr = sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) { sysmem->Release(); return hr; }

    IWICImagingFactory* factory = GetWicFactory();
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;

    hr = factory ? S_OK : E_FAIL;
    if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &props);
    if (SUCCEEDED(hr)) hr = frame->Initialize(props);
    if (SUCCEEDED(hr)) hr = frame->SetSize(desc.Width, desc.Height);
    if (SUCCEEDED(hr)) {
        WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&pixel_format);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(
            desc.Height,
            static_cast<UINT>(lr.Pitch),
            static_cast<UINT>(lr.Pitch) * desc.Height,
            static_cast<BYTE*>(lr.pBits));
    }
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();

    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();

    sysmem->UnlockRect();
    sysmem->Release();
    return hr;
}

}  // namespace

int DXOverlay::SaveGeometryToFile(
    const std::wstring& filename,
    float min_x, float min_y,
    float max_x, float max_y
) {
    IDirect3DDevice9* device = GW::render::GetDevice();
    if (!device) return ERR_NO_DEVICE;
    if (primitives.empty()) return ERR_NO_PRIMITIVES;

    float src_width = max_x - min_x;
    float src_height = max_y - min_y;
    if (src_width <= 0 || src_height <= 0) return ERR_INVALID_DIMENSIONS;

    // --- Scale to fit max 2048 while preserving aspect ratio
    float scale = 2048.0f / std::max(src_width, src_height);
    int out_width = static_cast<int>(src_width * scale);
    int out_height = static_cast<int>(src_height * scale);

    // --- Create render target texture
    IDirect3DTexture9* rtTexture = nullptr;
    HRESULT hr = device->CreateTexture(
        out_width, out_height, 1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &rtTexture, nullptr
    );
    if (FAILED(hr) || !rtTexture) return ERR_CREATE_TEXTURE;

    IDirect3DSurface9* rtSurface = nullptr;
    hr = rtTexture->GetSurfaceLevel(0, &rtSurface);
    if (FAILED(hr) || !rtSurface) { rtTexture->Release(); return ERR_GET_SURFACE; }

    IDirect3DSurface9* oldSurface = nullptr;
    hr = device->GetRenderTarget(0, &oldSurface);
    if (FAILED(hr)) { rtSurface->Release(); rtTexture->Release(); return ERR_GET_OLD_RT; }

    hr = device->SetRenderTarget(0, rtSurface);
    if (FAILED(hr)) { oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_SET_RT; }

    // --- State block save
    IDirect3DStateBlock9* state_block = nullptr;
    hr = device->CreateStateBlock(D3DSBT_ALL, &state_block);
    if (FAILED(hr)) { device->SetRenderTarget(0, oldSurface); oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_CREATE_STATEBLOCK; }

    D3DMATRIX old_world, old_view, old_proj;
    device->GetTransform(D3DTS_WORLD, &old_world);
    device->GetTransform(D3DTS_VIEW, &old_view);
    device->GetTransform(D3DTS_PROJECTION, &old_proj);

    // --- Viewport
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)out_width, (DWORD)out_height, 0.0f, 1.0f };
    hr = device->SetViewport(&vp);
    if (FAILED(hr)) {
        state_block->Release();
        device->SetRenderTarget(0, oldSurface);
        oldSurface->Release(); rtSurface->Release(); rtTexture->Release();
        return ERR_SET_VIEWPORT;
    }

    // --- Clear target
    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    // Render the primitive list scaled/offset to fit the output texture.
    for (const auto& shape : primitives) {
        if (shape.size() != 3 && shape.size() != 4) continue;

        D3DVertex verts[4];
        for (size_t i = 0; i < shape.size(); ++i) {
            float x = (shape[i].x - min_x) * scale;
            float y = (shape[i].y - min_y) * scale;

            verts[i] = { x, y, 0.0f, 1.0f, color };
        }

        if (shape.size() == 4)
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(D3DVertex));
        else
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(D3DVertex));
    }

    // --- Save to file (WIC PNG; legacy used D3DXSaveSurfaceToFileW)
    hr = SaveSurfaceToPngWIC(device, rtSurface, filename);

    // --- Restore state
    device->SetRenderTarget(0, oldSurface);
    device->SetTransform(D3DTS_WORLD, &old_world);
    device->SetTransform(D3DTS_VIEW, &old_view);
    device->SetTransform(D3DTS_PROJECTION, &old_proj);

    if (state_block) {
        HRESULT apply_hr = state_block->Apply();
        state_block->Release();
        if (FAILED(apply_hr)) { oldSurface->Release(); rtSurface->Release(); rtTexture->Release(); return ERR_STATEBLOCK_APPLY; }
    }

    // Cleanup
    if (oldSurface) oldSurface->Release();
    if (rtSurface) rtSurface->Release();
    if (rtTexture) rtTexture->Release();

    if (FAILED(hr)) return ERR_SAVE_FILE;
    return SAVE_OK;
}

}  // namespace PY4GW::overlay
