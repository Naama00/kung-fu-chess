#pragma once

#include <vector>
#include "realtime/Motion.hpp"
#include "common/GameConfig.hpp"

namespace kungfu {

struct MidRouteCollision {
    Motion winner;
    Motion loser;
};

// רכיב גיאומטרי טהור: אחראי אך ורק על איתור פיזי של חפיפות ונקודות מפגש
class CollisionDetector {
public:
    static std::vector<MidRouteCollision> detectMidRouteCollisions(
        const std::vector<Motion>& activeMotions,
        const GameConfig& config
    ) noexcept;

    static std::vector<Motion> detectArrivals(
        const std::vector<Motion>& activeMotions,
        int currentTimeMs
    ) noexcept;
};

} // namespace kungfu