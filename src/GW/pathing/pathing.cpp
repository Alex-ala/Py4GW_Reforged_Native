#include "base/error_handling.h"

#include "GW/pathing/pathing.h"

#include "base/logger.h"
#include "GW/game_thread/game_thread.h"

#include <array>

namespace GW::pathing {

PathPlanner::PathPlanner() {
    ResolveFindPathFunc();
}

void PathPlanner::PlanPath(float start_x, float start_y, float start_z,
    float goal_x, float goal_y, float goal_z) {
    status.store(Status::Pending);
    planned_path.clear();

    if (!ResolveFindPathFunc()) {
        Logger::Instance().LogError("FindPath_Func not initialized.", "pathing");
        status.store(Status::Failed);
        return;
    }

    const GW::GamePos start_pos(start_x, start_y, static_cast<uint32_t>(start_z));
    const GW::GamePos goal_pos(goal_x, goal_y, static_cast<uint32_t>(goal_z));

    GW::game_thread::Enqueue([this, start_pos, goal_pos]() {
        std::array<PathPoint, 30> pathArray;
        uint32_t pathCount = 0;
        uint32_t maxPoints = static_cast<uint32_t>(pathArray.size());

        PathPoint start{ start_pos, nullptr };
        PathPoint goal{ goal_pos, nullptr };

        g_find_path_func(&start, &goal, 10000.0f, maxPoints, &pathCount, pathArray.data());

        planned_path.clear();
        if (pathCount == 0 ||
            (pathCount == 1 && (pathArray[0].pos.x != goal_pos.x || pathArray[0].pos.y != goal_pos.y))) {
            status.store(Status::Failed);
            return;
        }

        for (uint32_t i = 0; i < pathCount; ++i) {
            GW::GamePos& p = pathArray[i].pos;

            planned_path.emplace_back(p.x, p.y, static_cast<float>(p.zplane));
        }

        status.store(Status::Ready);
    });
}

std::vector<std::tuple<float, float, float>> PathPlanner::ComputePathImmediate(float start_x, float start_y, float start_z,
    float goal_x, float goal_y, float goal_z) {
    std::vector<std::tuple<float, float, float>> result;

    if (!ResolveFindPathFunc()) {
        Logger::Instance().LogError("FindPath_Func not initialized.", "pathing");
        return result;
    }

    const GW::GamePos start_pos(start_x, start_y, static_cast<uint32_t>(start_z));
    const GW::GamePos goal_pos(goal_x, goal_y, static_cast<uint32_t>(goal_z));

    std::array<PathPoint, 30> pathArray;
    uint32_t pathCount = 0;
    uint32_t maxPoints = static_cast<uint32_t>(pathArray.size());

    PathPoint start{ start_pos, nullptr };
    PathPoint goal{ goal_pos, nullptr };

    g_find_path_func(&start, &goal, 10000.0f, maxPoints, &pathCount, pathArray.data());

    if (pathCount == 0 ||
        (pathCount == 1 && (pathArray[0].pos.x != goal_pos.x || pathArray[0].pos.y != goal_pos.y))) {
        status.store(Status::Failed);
        return result;
    }

    for (uint32_t i = 0; i < pathCount; ++i) {
        const GW::GamePos& p = pathArray[i].pos;
        result.emplace_back(p.x, p.y, static_cast<float>(p.zplane));
    }

    return result;
}

}  // namespace GW::pathing
