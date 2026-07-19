#pragma once

#include "base/error_handling.h"

#include <cstdint>
#include <memory>
#include <vector>

struct IDirect3DDevice9;

// Generic GPU-drawn particle system (reusable, not beam-specific).
// -----------------------------------------------------------------------------
// C++ owns the simulation; Python only configures. An emitter is a pool of
// particles with a config struct exposing every knob (def_readwrite in the
// bindings). The system registers ONE world-pass draw (occluded), advances every
// live emitter each frame, and draws them as camera-facing billboard quads through
// a small colored vertex/pixel shader (no textures, additive or alpha blend).
//
// Lifetime is by ownership: CreateEmitter() returns a shared_ptr the caller (Python)
// holds; the system keeps a weak_ptr. Drop the handle (or the script stops) and the
// emitter is destroyed and stops drawing - no leak, no keepalive to manage.

namespace PY4GW::particles {

enum EmitMode : int { EMIT_BALLISTIC = 0, EMIT_ORBITAL = 1 };

// Every field here is a Python-settable control. Positions/velocities are WORLD
// space in GW's convention (up = -z).
struct EmitterConfig {
    bool  enabled = true;
    int   mode = EMIT_BALLISTIC;
    int   max_particles = 800;   // hard pool cap
    float rate = 40.0f;          // continuous emission, particles/second (0 = burst only)

    // emitter origin (world). Set every frame if you attach it to a moving thing.
    float origin_x = 0.0f, origin_y = 0.0f, origin_z = 0.0f;

    // --- ballistic launch (mode = EMIT_BALLISTIC) ---
    float dir_x = 0.0f, dir_y = 0.0f, dir_z = -1.0f;  // launch direction; up = -z
    float speed = 150.0f, speed_var = 60.0f;          // launch speed +/- variance
    float spread = 0.45f;                             // cone half-angle (radians)

    // physics
    float grav_x = 0.0f, grav_y = 0.0f, grav_z = 280.0f;  // +z pulls down (up = -z)
    float drag = 0.0f;                                    // velocity damping per second

    // --- orbital (mode = EMIT_ORBITAL): particles spiral around the origin axis ---
    float orbit_radius = 30.0f, orbit_radius_var = 12.0f;
    float orbit_spin = 2.0f;      // radians/second around the vertical axis
    float orbit_rise = 40.0f;     // climb speed (world units/second, upward)
    float orbit_height = 260.0f;  // climb this far, then recycle

    // lifetime
    float life = 1.5f, life_var = 0.5f;   // seconds (ballistic). orbital also honors orbit_height.

    // --- extra shaping (these unlock most of the effect palette) ---
    float spawn_radius = 0.0f;       // spawn within a ground disc of this radius around origin
    float radial_speed = 0.0f;       // extra outward (horizontal) launch speed from origin
    float orbit_radius_end = -1.0f;  // orbital: radius at end of life (<0 = constant orbit_radius)
    float turbulence = 0.0f;         // random velocity jitter, world units/sec^2 (organic wander)
    float stretch = 0.0f;            // velocity-aligned streak length factor (0 = round billboard)

    // appearance (size in world units; color argb 0xAARRGGBB, interpolated over life)
    float size = 2.5f, size_var = 1.0f, size_end = 0.0f;
    uint32_t color = 0xFFFFFFFFu, color_end = 0x00FFFFFFu;
    float hot_frac = 0.4f;        // fraction of particles spawned white-hot
    bool  additive = true;        // additive (light) vs. alpha blend
};

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float age, life;
    float size0, size1;
    uint32_t c0, c1;
    // orbital state
    float theta, radius, radius1, spin, base_z;
    bool active = false;
};

// A colored vertex for the shader draw (matches FVF XYZ|DIFFUSE).
struct PVertex { float x, y, z; uint32_t argb; };

class ParticleEmitter {
public:
    EmitterConfig config;

    void SetOrigin(float x, float y, float z);  // move origin + stamp liveness (feeds the timeout)
    void Keepalive();          // stamp liveness without moving (keep a static effect alive)
    bool TimedOut(unsigned long now_ms, unsigned long timeout_ms) const;  // no feed since timeout?
    void Emit(int n);          // spawn n particles immediately (burst)
    void Clear();              // kill all particles
    void Update(float dt);     // continuous emission + integration
    size_t Count() const;      // live particle count (diagnostics)

    // Append camera-facing billboard quads (triangle list, 6 verts/particle) for the
    // current particle state. right/up are the camera's world-space screen axes.
    void AppendQuads(std::vector<PVertex>& out,
                     float rx, float ry, float rz,
                     float ux, float uy, float uz) const;

private:
    std::vector<Particle> pool_;
    float accum_ = 0.0f;
    unsigned long last_fed_ms_ = 0;  // GetTickCount at last SetOrigin/Keepalive (feed timeout, like DXOverlay)
    uint32_t rng_ = 0x2545F491u;
    float Rnd();                       // [0,1)
    Particle* Spawn();
};

// Create a system-managed emitter. The system weak-refs it; the returned shared_ptr
// is the owner (hold it to keep the effect alive; drop it to stop + free).
std::shared_ptr<ParticleEmitter> CreateEmitter();

// Remove every emitter and unregister the draw (e.g. on shutdown).
void Shutdown();

}  // namespace PY4GW::particles
