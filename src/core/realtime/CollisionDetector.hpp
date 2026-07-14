#pragma once

#include <vector>
#include <optional>
#include "core/realtime/Motion.hpp"
#include "core/common/GameConfig.hpp"

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

private:
    // מתודת עזר פנימית לביצוע בדיקה מעמיקה (Narrow Phase) של התנגשות בין שני מסלולים
    static std::optional<MidRouteCollision> checkDetailedCollision(
        const Motion& m1, 
        const Motion& m2
    ) noexcept;
};

} // namespace kungfu