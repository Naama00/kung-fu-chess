#include "realtime/RealTimeArbiter.hpp"
#include "realtime/CollisionDetector.hpp"
#include "rules/CollisionResolver.hpp"
#include "common/GameConfig.hpp"
#include <algorithm>

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board, GameConfig config) noexcept
    : board_(std::move(board)), config_(std::move(config)) {}

bool RealTimeArbiter::hasActiveMotion() const noexcept {
    return !activeMotions_.empty();
}

void RealTimeArbiter::startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept {
    activeMotions_.emplace_back(piece, from, to, currentTimeMs, durationMs);
    // העברת הכלי למיקום זמני חסר השפעה כדי לפנות מיד את משבצת המוצא שלו
    piece->setPosition(Position(-1, -1));
}

std::vector<ArrivalEvent> RealTimeArbiter::advanceTime(int ms, int& currentTimeMs, PromotionHandler promoteCallback) noexcept {
    std::vector<ArrivalEvent> events;
    currentTimeMs += ms;

    CollisionResolver resolver(board_, cooldownTracker_, config_);

    // 1. איתור ופתרון התנגשויות באמצע הדרך
    auto midRouteCollisions = CollisionDetector::detectMidRouteCollisions(activeMotions_, config_);
    for (const auto& col : midRouteCollisions) {
        if (col.winner.piece()->state() != PieceState::Captured &&
            col.loser.piece()->state() != PieceState::Captured) {
            resolver.resolveMidRouteCollision(col.winner, col.loser, events);
        }
    }

    // ניקוי כלים שחוסלו באמצע הדרך מרשימת התנועות הפעילות
    activeMotions_.erase(
        std::remove_if(activeMotions_.begin(), activeMotions_.end(),
            [](const Motion& m) { return m.piece()->state() == PieceState::Captured; }),
        activeMotions_.end()
    );

    // 2. איתור, פיצול ומיון כרונולוגי של הגעות ליעד
    std::vector<Motion> dueMotions;
    std::vector<Motion> remainingMotions;
    dueMotions.reserve(activeMotions_.size());
    remainingMotions.reserve(activeMotions_.size());

    for (auto& m : activeMotions_) {
        if (currentTimeMs >= m.arrivalTime()) {
            dueMotions.push_back(std::move(m));
        } else {
            remainingMotions.push_back(std::move(m));
        }
    }
    activeMotions_ = std::move(remainingMotions);

    // מיון יציב כרונולוגי של הגעות
    std::stable_sort(dueMotions.begin(), dueMotions.end(), [](const Motion& a, const Motion& b) noexcept {
        return a.arrivalTime() < b.arrivalTime();
    });

    // פתרון הגעות לפי סדר כרונולוגי מדויק
    for (const auto& motion : dueMotions) {
        if (motion.piece()->state() == PieceState::Captured) {
            continue;
        }

        if (!resolver.resolveArrival(motion, currentTimeMs, events, promoteCallback)) {
            // תרחיש תיאורטי בו כלי לא נחת וצריך לחזור לרשימה
            activeMotions_.push_back(motion);
        }
    }

    return events;
}

bool RealTimeArbiter::isOnCooldown(const PiecePtr& piece, int currentTimeMs) const noexcept {
    if (!piece) return false;
    return cooldownTracker_.isOnCooldown(piece->id(), currentTimeMs);
}

bool RealTimeArbiter::isPieceMoving(const PiecePtr& piece) const noexcept {
    if (!piece) return false;
    for (const auto& motion : activeMotions_) {
        if (motion.piece() == piece) {
            return true;
        }
    }
    return false;
}

std::optional<Motion> RealTimeArbiter::getMotionForPiece(const PiecePtr& piece) const noexcept {
    if (!piece) {
        return std::nullopt;
    }
    for (const auto& motion : activeMotions_) {
        if (motion.piece() == piece) {
            return motion;
        }
    }
    return std::nullopt;
}

std::optional<PiecePtr> RealTimeArbiter::getPieceInTransitAt(const Position& pos) const noexcept {
    for (const auto& m : activeMotions_) {
        if (m.from() == pos || m.to() == pos) {
            return m.piece();
        }
    }
    return std::nullopt;
}

}  // namespace kungfu