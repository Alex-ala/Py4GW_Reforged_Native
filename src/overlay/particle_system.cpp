#include "base/error_handling.h"

#include "overlay/particle_system.h"

#include "GW/context/camera.h"
#include "GW/context/context.h"
#include "GW/map/map.h"
#include "GW/render/render.h"
#include "GW/world_render/world_render.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include <windows.h>
#include <DirectXMath.h>
#include <d3d9.h>
#include <d3dcompiler.h>

using namespace DirectX;

namespace PY4GW::particles {

namespace {

constexpr float kNear = 48000.0f / 1024.0f;  // 46.875, matches the overlay depth convention
constexpr float kFar = 48000.0f;

// Feed timeout, same rule as the DXOverlay occlusion class: if an emitter hasn't been
// fed (SetOrigin / Keepalive) for this long, it clears + stops (resumes when fed again).
constexpr unsigned long kFeedTimeoutMs = 100;

uint32_t LerpARGB(uint32_t a, uint32_t b, float t) {
    if (t < 0) t = 0; else if (t > 1) t = 1;
    const int a0 = (a >> 24) & 0xFF, r0 = (a >> 16) & 0xFF, g0 = (a >> 8) & 0xFF, b0 = a & 0xFF;
    const int a1 = (b >> 24) & 0xFF, r1 = (b >> 16) & 0xFF, g1 = (b >> 8) & 0xFF, b1 = b & 0xFF;
    const int A = a0 + int((a1 - a0) * t);
    const int R = r0 + int((r1 - r0) * t);
    const int G = g0 + int((g1 - g0) * t);
    const int B = b0 + int((b1 - b0) * t);
    return (uint32_t(A) << 24) | (uint32_t(R) << 16) | (uint32_t(G) << 8) | uint32_t(B);
}

bool MapValid() {
    return GW::map::GetIsMapLoaded() &&
           GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;
}

// ---- system state (render-thread) ----
std::mutex g_mutex;
std::vector<std::weak_ptr<ParticleEmitter>> g_emitters;
int g_token = -1;
bool g_registered = false;
unsigned long g_last_ms = 0;

IDirect3DVertexShader9* g_vs = nullptr;
IDirect3DPixelShader9* g_ps = nullptr;
IDirect3DDevice9* g_shader_device = nullptr;
std::vector<PVertex> g_scratch;

bool EnsureShaders(IDirect3DDevice9* device) {
    if (g_shader_device != device) {
        if (g_vs) { g_vs->Release(); g_vs = nullptr; }
        if (g_ps) { g_ps->Release(); g_ps = nullptr; }
        g_shader_device = device;
    }
    if (g_vs && g_ps) return true;
    static const char vs_src[] =
        "float4x4 gView : register(c0);\n"
        "float4x4 gProj : register(c4);\n"
        "struct VSIn  { float4 pos : POSITION; float4 col : COLOR0; };\n"
        "struct VSOut { float4 pos : POSITION; float4 col : COLOR0; };\n"
        "VSOut main(VSIn i){ VSOut o; float4 v = mul(i.pos, gView);"
        " o.pos = mul(v, gProj); o.col = i.col; return o; }\n";
    static const char ps_src[] =
        "struct PSIn { float4 col : COLOR0; };\n"
        "float4 main(PSIn i) : COLOR0 { return i.col; }\n";
    ID3DBlob* blob = nullptr; ID3DBlob* err = nullptr;
    if (SUCCEEDED(D3DCompile(vs_src, sizeof(vs_src) - 1, nullptr, nullptr, nullptr, "main", "vs_3_0", 0, 0, &blob, &err)) && blob)
        device->CreateVertexShader(static_cast<const DWORD*>(blob->GetBufferPointer()), &g_vs);
    if (blob) { blob->Release(); blob = nullptr; } if (err) { err->Release(); err = nullptr; }
    if (SUCCEEDED(D3DCompile(ps_src, sizeof(ps_src) - 1, nullptr, nullptr, nullptr, "main", "ps_3_0", 0, 0, &blob, &err)) && blob)
        device->CreatePixelShader(static_cast<const DWORD*>(blob->GetBufferPointer()), &g_ps);
    if (blob) blob->Release(); if (err) err->Release();
    return g_vs && g_ps;
}

// Set the shared shader + matrices + depth once per frame; returns the camera's
// world-space screen right/up axes (for billboarding) via out params.
bool SetupDraw(IDirect3DDevice9* device, float* right, float* up) {
    if (!EnsureShaders(device)) return false;
    auto* cam = GW::Context::GetCamera();
    if (!cam) return false;

    const float ex = cam->position.x, ey = cam->position.y, ez = cam->position.z;
    const float ax = cam->look_at_target.x, ay = cam->look_at_target.y, az = cam->look_at_target.z;
    float fx = ax - ex, fy = ay - ey, fz = az - ez;
    float fl = std::sqrt(fx*fx + fy*fy + fz*fz); if (fl < 1e-5f) return false;
    fx/=fl; fy/=fl; fz/=fl;
    // Camera screen axes in world space. worldUp = (0,0,-1) (GW up = -z).
    // right = normalize(cross(worldUp, fwd)) = normalize(fy, -fx, 0).
    float rx = fy, ry = -fx, rz = 0.0f;
    float rl = std::sqrt(rx*rx + ry*ry + rz*rz); if (rl < 1e-5f) { rx = 1; ry = 0; rz = 0; rl = 1; }
    rx/=rl; ry/=rl; rz/=rl;
    // up = cross(fwd, right)
    const float ux = fy*rz - fz*ry, uy = fz*rx - fx*rz, uz = fx*ry - fy*rx;
    right[0]=rx; right[1]=ry; right[2]=rz; up[0]=ux; up[1]=uy; up[2]=uz;

    XMFLOAT3 eye(ex,ey,ez), at(ax,ay,az), upv(0.0f,0.0f,-1.0f);
    XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&at), XMLoadFloat3(&upv));
    const float fov = GW::render::GetFieldOfView();
    const float aspect = static_cast<float>(GW::render::GetViewportWidth()) /
                         static_cast<float>(GW::render::GetViewportHeight());
    XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, aspect, kNear, kFar);
    XMMATRIX vt = XMMatrixTranspose(view), pt = XMMatrixTranspose(proj);

    device->SetVertexShader(g_vs);
    device->SetPixelShader(g_ps);
    device->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(&vt), 4);
    device->SetVertexShaderConstantF(4, reinterpret_cast<const float*>(&pt), 4);
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    device->SetTexture(0, nullptr);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    return true;
}

// The ONE registered world-pass callback: advance + draw every live emitter.
void Tick(IDirect3DDevice9* device) {
    if (!device || !MapValid()) return;

    const unsigned long now = GetTickCount();
    float dt = g_last_ms ? (now - g_last_ms) / 1000.0f : 0.0f;
    g_last_ms = now;
    if (dt > 0.05f) dt = 0.05f;  // clamp hitches

    float right[3], up[3];
    if (!SetupDraw(device, right, up)) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_emitters.begin(); it != g_emitters.end();) {
        auto em = it->lock();
        if (!em) { it = g_emitters.erase(it); continue; }  // owner dropped it -> gone
        if (em->TimedOut(now, kFeedTimeoutMs)) {           // not fed for 100ms -> dormant
            em->Clear();
            ++it;
            continue;
        }
        em->Update(dt);  // integrate always; Update() gates emission by `enabled`

        g_scratch.clear();
        em->AppendQuads(g_scratch, right[0], right[1], right[2], up[0], up[1], up[2]);
        if (!g_scratch.empty()) {
            device->SetRenderState(D3DRS_DESTBLEND, em->config.additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(g_scratch.size() / 3),
                                    g_scratch.data(), sizeof(PVertex));
        }
        ++it;
    }
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
}

void EnsureRegistered() {
    if (g_registered) return;
    g_token = GW::world_render::RegisterDraw([](IDirect3DDevice9* d) { Tick(d); });
    g_registered = (g_token >= 0);
    g_last_ms = 0;
}

}  // namespace

// ---- ParticleEmitter ----
void ParticleEmitter::SetOrigin(float x, float y, float z) {
    config.origin_x = x; config.origin_y = y; config.origin_z = z;
    last_fed_ms_ = GetTickCount();
}

void ParticleEmitter::Keepalive() { last_fed_ms_ = GetTickCount(); }

bool ParticleEmitter::TimedOut(unsigned long now_ms, unsigned long timeout_ms) const {
    return (now_ms - last_fed_ms_) > timeout_ms;
}

float ParticleEmitter::Rnd() {
    uint32_t x = rng_; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rng_ = x;
    return (x & 0xFFFFFFu) / static_cast<float>(0x1000000);
}

Particle* ParticleEmitter::Spawn() {
    for (auto& p : pool_) if (!p.active) return &p;
    if (static_cast<int>(pool_.size()) < config.max_particles) { pool_.emplace_back(); return &pool_.back(); }
    return nullptr;  // pool at cap
}

void ParticleEmitter::Emit(int n) {
    for (int i = 0; i < n; ++i) {
        Particle* pp = Spawn();
        if (!pp) break;
        Particle& p = *pp;
        p.active = true; p.age = 0.0f;
        p.life = config.life + (Rnd() * 2 - 1) * config.life_var; if (p.life < 0.05f) p.life = 0.05f;
        p.size0 = config.size + (Rnd() * 2 - 1) * config.size_var; if (p.size0 < 0.1f) p.size0 = 0.1f;
        p.size1 = config.size_end;
        const bool hot = Rnd() < config.hot_frac;
        p.c0 = hot ? 0xFFFFFFFFu : config.color;
        p.c1 = hot ? 0x00FFFFFFu : config.color_end;
        // spawn position: origin + a random point in a ground disc (spawn_radius)
        float sox = 0.0f, soy = 0.0f;
        if (config.spawn_radius > 0.0f) {
            const float sa = Rnd() * 6.2831853f;
            const float sr = config.spawn_radius * std::sqrt(Rnd());  // uniform over the disc
            sox = std::cos(sa) * sr; soy = std::sin(sa) * sr;
        }
        if (config.mode == EMIT_ORBITAL) {
            p.theta = Rnd() * 6.2831853f;
            p.radius = config.orbit_radius + (Rnd() * 2 - 1) * config.orbit_radius_var;
            p.radius1 = config.orbit_radius_end < 0.0f ? p.radius : config.orbit_radius_end;
            p.spin = config.orbit_spin;
            p.base_z = config.origin_z; p.z = config.origin_z;
            p.x = config.origin_x + std::cos(p.theta) * p.radius;
            p.y = config.origin_y + std::sin(p.theta) * p.radius;
            p.vx = p.vy = p.vz = 0.0f;
        } else {
            p.x = config.origin_x + sox; p.y = config.origin_y + soy; p.z = config.origin_z;
            // launch direction within a cone of half-angle `spread` (0=beam .. pi=full sphere)
            float dx = config.dir_x, dy = config.dir_y, dz = config.dir_z;
            float dl = std::sqrt(dx*dx + dy*dy + dz*dz); if (dl < 1e-5f) { dx = 0; dy = 0; dz = -1; dl = 1; }
            dx/=dl; dy/=dl; dz/=dl;
            float px, py, pz;
            if (std::fabs(dz) < 0.99f) { px = -dy; py = dx; pz = 0.0f; } else { px = 1.0f; py = 0.0f; pz = 0.0f; }
            float pl = std::sqrt(px*px + py*py + pz*pz); px/=pl; py/=pl; pz/=pl;
            const float qx = dy*pz - dz*py, qy = dz*px - dx*pz, qz = dx*py - dy*px;  // q = d x p
            const float cosA = 1.0f - Rnd() * (1.0f - std::cos(config.spread));       // uniform in cone
            const float sinA = std::sqrt(std::max(0.0f, 1.0f - cosA * cosA));
            const float phi = Rnd() * 6.2831853f;
            const float lx = sinA * std::cos(phi), ly = sinA * std::sin(phi), lz = cosA;
            const float wx = lx*px + ly*qx + lz*dx;   // already a unit world direction
            const float wy = lx*py + ly*qy + lz*dy;
            const float wz = lx*pz + ly*qz + lz*dz;
            const float sp = config.speed + (Rnd() * 2 - 1) * config.speed_var;
            p.vx = wx * sp; p.vy = wy * sp; p.vz = wz * sp;
            // extra outward horizontal speed from the origin (shockwaves / novas)
            if (config.radial_speed != 0.0f) {
                float rdx = sox, rdy = soy, rl = std::sqrt(rdx*rdx + rdy*rdy);
                if (rl < 1e-4f) { const float ra = Rnd() * 6.2831853f; rdx = std::cos(ra); rdy = std::sin(ra); rl = 1; }
                p.vx += rdx / rl * config.radial_speed;
                p.vy += rdy / rl * config.radial_speed;
            }
        }
    }
}

void ParticleEmitter::Clear() {
    for (auto& p : pool_) p.active = false;
}

void ParticleEmitter::Update(float dt) {
    if (dt <= 0.0f) return;
    // Emission is gated by `enabled`; integration always runs so a disabled emitter's
    // existing particles finish their life and fade instead of freezing in mid-air.
    if (config.enabled && config.rate > 0.0f) {
        accum_ += config.rate * dt;
        int n = static_cast<int>(accum_);
        if (n > 0) { accum_ -= n; Emit(n); }
    }
    const float gx = config.grav_x, gy = config.grav_y, gz = config.grav_z;
    const float damp = 1.0f - config.drag * dt;
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.age += dt;
        if (config.mode == EMIT_ORBITAL) {
            const float tt = p.life > 0 ? p.age / p.life : 1.0f;
            const float cr = p.radius + (p.radius1 - p.radius) * (tt < 1 ? tt : 1);  // radius over life
            p.theta += p.spin * dt;
            p.z -= config.orbit_rise * dt;
            p.x = config.origin_x + std::cos(p.theta) * cr;
            p.y = config.origin_y + std::sin(p.theta) * cr;
            if (p.age >= p.life || (p.base_z - p.z) >= config.orbit_height) p.active = false;
        } else {
            if (config.turbulence > 0.0f) {
                p.vx += (Rnd() * 2 - 1) * config.turbulence * dt;
                p.vy += (Rnd() * 2 - 1) * config.turbulence * dt;
                p.vz += (Rnd() * 2 - 1) * config.turbulence * dt;
            }
            p.vx = (p.vx + gx*dt) * damp;
            p.vy = (p.vy + gy*dt) * damp;
            p.vz = (p.vz + gz*dt) * damp;
            p.x += p.vx*dt; p.y += p.vy*dt; p.z += p.vz*dt;
            if (p.age >= p.life) p.active = false;
        }
    }
}

size_t ParticleEmitter::Count() const {
    size_t n = 0; for (const auto& p : pool_) if (p.active) ++n; return n;
}

void ParticleEmitter::AppendQuads(std::vector<PVertex>& out,
                                  float rx, float ry, float rz,
                                  float ux, float uy, float uz) const {
    const float stretch = config.stretch;
    for (const auto& p : pool_) {
        if (!p.active) continue;
        const float t = p.life > 0 ? p.age / p.life : 1.0f;
        const float s = p.size0 + (p.size1 - p.size0) * (t < 1 ? t : 1);
        const uint32_t c = LerpARGB(p.c0, p.c1, t);

        float ax, ay, az, bx, by, bz;   // the two quad half-axes
        const float vlen = std::sqrt(p.vx*p.vx + p.vy*p.vy + p.vz*p.vz);
        if (stretch > 0.0f && vlen > 1e-3f) {
            // velocity-aligned streak: long axis along motion, short axis = camera-right made
            // perpendicular to it (so the streak stays screen-readable).
            const float vx = p.vx/vlen, vy = p.vy/vlen, vz = p.vz/vlen;
            const float half_len = s + stretch * vlen;
            ax = vx*half_len; ay = vy*half_len; az = vz*half_len;
            const float dotv = rx*vx + ry*vy + rz*vz;
            float sx = rx - dotv*vx, sy = ry - dotv*vy, sz = rz - dotv*vz;
            float sl = std::sqrt(sx*sx + sy*sy + sz*sz);
            if (sl < 1e-4f) { sx = ux; sy = uy; sz = uz; sl = 1; }
            bx = sx/sl*s; by = sy/sl*s; bz = sz/sl*s;
        } else {
            ax = rx*s; ay = ry*s; az = rz*s;   // round billboard: right * size
            bx = ux*s; by = uy*s; bz = uz*s;   // up * size
        }
        const PVertex v0{ p.x - ax - bx, p.y - ay - by, p.z - az - bz, c };
        const PVertex v1{ p.x + ax - bx, p.y + ay - by, p.z + az - bz, c };
        const PVertex v2{ p.x + ax + bx, p.y + ay + by, p.z + az + bz, c };
        const PVertex v3{ p.x - ax + bx, p.y - ay + by, p.z - az + bz, c };
        out.push_back(v0); out.push_back(v1); out.push_back(v2);
        out.push_back(v0); out.push_back(v2); out.push_back(v3);
    }
}

// ---- system entry points ----
std::shared_ptr<ParticleEmitter> CreateEmitter() {
    auto em = std::make_shared<ParticleEmitter>();
    em->Keepalive();  // fresh emitter isn't instantly timed out before its first feed
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_emitters.push_back(em);
    }
    EnsureRegistered();
    return em;
}

void Shutdown() {
    if (g_registered && g_token >= 0) {
        GW::world_render::UnregisterDraw(g_token);
        g_token = -1;
    }
    g_registered = false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_emitters.clear();
}

}  // namespace PY4GW::particles
