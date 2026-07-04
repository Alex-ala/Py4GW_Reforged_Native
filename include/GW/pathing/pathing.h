#pragma once

#include "base/error_handling.h"

#include "GW/context/pathing.h"

#include <atomic>
#include <cstdint>
#include <tuple>
#include <vector>

// Pathfinding module migrated from legacy py_pathing_maps.h (PathPlanner over
// the engine FindPath function). No lifecycle step: the FindPath symbol is
// resolved lazily on first use (same shape as the map module's raycast
// bridge); nothing is hooked or patched.
namespace GW::pathing {

/* ---------------- Resolved-symbol surface (module-owned) ---------------- */

using PathPoint = Context::PathPoint;

typedef void(__cdecl* FindPath_pt)(PathPoint* start, PathPoint* goal, float range, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);

extern FindPath_pt g_find_path_func;

// Resolver ownership (body in pathing_patterns.cpp); byte pattern lives in
// offsets/pathing.json. Idempotent, lazily invoked.
bool ResolveFindPathFunc();

/* ---------------- PathPlanner (legacy surface) ---------------- */

class PathPlanner {
public:
    enum class Status {
        Idle,
        Pending,
        Ready,
        Failed
    };

    PathPlanner();

    void PlanPath(float start_x, float start_y, float start_z,
        float goal_x, float goal_y, float goal_z);

    std::vector<std::tuple<float, float, float>> ComputePathImmediate(float start_x, float start_y, float start_z,
        float goal_x, float goal_y, float goal_z);

    Status GetStatus() const {
        return status.load();
    }

    bool IsReady() const {
        return status.load() == Status::Ready;
    }

    bool WasSuccessful() const {
        Status s = status.load();
        return s == Status::Ready;
    }

    const std::vector<std::tuple<float, float, float>>& GetPath() const {
        return planned_path;
    }

    void Reset() {
        planned_path.clear();
        status.store(Status::Idle);
    }

private:
    std::vector<std::tuple<float, float, float>> planned_path;
    std::atomic<Status> status = Status::Idle;
};

}  // namespace GW::pathing
