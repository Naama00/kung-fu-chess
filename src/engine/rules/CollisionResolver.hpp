#pragma once

#include <memory>
#include <vector>
#include <functional>
#include "engine/board/IBoard.hpp"
#include "engine/realtime/Motion.hpp"
#include "engine/realtime/CooldownTracker.hpp"
#include "engine/realtime/RealTimeArbiter.hpp"
#include "engine/common/GameConfig.hpp"

namespace kungfu {

using PromotionHandler = std::function<PiecePtr(const PiecePtr&, const Position&)>;

class CollisionResolver {
public:
    CollisionResolver(
        std::shared_ptr<IBoard> board, 
        CooldownTracker& cooldownTracker, 
        const GameConfig& config
    ) noexcept;

    void resolveMidRouteCollision(
        const Motion& winner, // הגיע ראשון (המוקדם)
        const Motion& loser,  // הגיע שני (המאוחר)
        int currentTimeMs,    // נוסף לצורך פתרון הצינון של המהלך הידידותי
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