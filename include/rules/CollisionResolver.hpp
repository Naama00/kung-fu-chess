#pragma once

#include <memory>
#include <vector>
#include <functional>
#include "board/IBoard.hpp"
#include "realtime/Motion.hpp"
#include "realtime/CooldownTracker.hpp"
#include "realtime/RealTimeArbiter.hpp" // צורך את ArrivalEvent
#include "common/GameConfig.hpp"

namespace kungfu {

using PromotionHandler = std::function<PiecePtr(const PiecePtr&, const Position&)>;

// רכיב החוקים הטהור: מיישם את השלכות השחמט על הלוח ועל הכלים בעקבות התנגשויות
class CollisionResolver {
public:
    CollisionResolver(
        std::shared_ptr<IBoard> board, 
        CooldownTracker& cooldownTracker, 
        const GameConfig& config
    ) noexcept;

    void resolveMidRouteCollision(
        const Motion& winner,
        const Motion& loser,
        std::vector<ArrivalEvent>& events
    ) noexcept;

    bool resolveArrival(
        const Motion& motion,
        int currentTimeMs,
        std::vector<ArrivalEvent>& events,
        const PromotionHandler& promoteCallback
    ) noexcept;

private:
    std::shared_ptr<IBoard> board_;
    CooldownTracker& cooldownTracker_;
    GameConfig config_;
};

} // namespace kungfu