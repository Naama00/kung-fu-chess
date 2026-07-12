#include "realtime/CollisionDetector.hpp"

namespace kungfu {

std::vector<MidRouteCollision> CollisionDetector::detectMidRouteCollisions(
    const std::vector<Motion>& activeMotions,
    const GameConfig& config
) noexcept {
    std::vector<MidRouteCollision> collisions;
    if (!config.allowSimultaneousMovement || activeMotions.size() < 2) {
        return collisions;
    }

    for (size_t i = 0; i < activeMotions.size(); ++i) {
        for (size_t j = i + 1; j < activeMotions.size(); ++j) {
            const auto& m1 = activeMotions[i];
            const auto& m2 = activeMotions[j];

            if (m1.piece()->color() != m2.piece()->color()) {
                // פרשים קופצים באוויר ואינם מתנגשים באמצע הדרך
                if (m1.piece()->type() == PieceType::Knight || m2.piece()->type() == PieceType::Knight) {
                    continue;
                }

                // בדיקת התנגשות ראש בראש (החלפת מקומות)
                if (m1.from() == m2.to() && m1.to() == m2.from()) {
                    if (m1.startTime() <= m2.startTime()) {
                        collisions.push_back({m1, m2});
                    } else {
                        collisions.push_back({m2, m1});
                    }
                }
            }
        }
    }
    return collisions;
}

std::vector<Motion> CollisionDetector::detectArrivals(
    const std::vector<Motion>& activeMotions,
    int currentTimeMs
) noexcept {
    std::vector<Motion> arrivals;
    for (const auto& m : activeMotions) {
        if (currentTimeMs >= m.arrivalTime()) {
            arrivals.push_back(m);
        }
    }
    return arrivals;
}

} // namespace kungfu