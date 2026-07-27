// src/realtime/RealTimeArbiter.cpp
#include "engine/realtime/RealTimeArbiter.hpp"
#include "engine/realtime/CollisionDetector.hpp"
#include "engine/rules/CollisionResolver.hpp"
#include "engine/common/GameConfig.hpp"
#include <algorithm>
#include <cmath>

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board, GameConfig config) noexcept
    : board_(std::move(board)), config_(std::move(config)) {}

bool RealTimeArbiter::hasActiveMotion() const noexcept {
    return !activeMotions_.empty();
}

void RealTimeArbiter::startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept {
    activeMotions_.emplace_back(piece, from, to, currentTimeMs, durationMs);
}

void RealTimeArbiter::cancelMotionForPiece(const PiecePtr& piece) noexcept {
    if (!piece) return;
    activeMotions_.erase(
        std::remove_if(activeMotions_.begin(), activeMotions_.end(),
            [&piece](const Motion& m) noexcept {
                return m.piece() == piece;
            }),
        activeMotions_.end()
    );
}

std::vector<ArrivalEvent> RealTimeArbiter::advanceTime(int ms, int& currentTimeMs, PromotionHandler promoteCallback) noexcept {
    std::vector<ArrivalEvent> events;
    currentTimeMs += ms;

    CollisionResolver resolver(board_, cooldownTracker_, config_);

    auto midRouteCollisions = CollisionDetector::detectMidRouteCollisions(activeMotions_, config_);
    for (const auto& col : midRouteCollisions) {
        if (col.winner.piece()->state() != PieceState::Captured &&
            col.loser.piece()->state() != PieceState::Captured) {
            resolver.resolveMidRouteCollision(col.winner, col.loser, currentTimeMs, events);
        }
    }

    activeMotions_.erase(
        std::remove_if(activeMotions_.begin(), activeMotions_.end(),
            [](const Motion& m) {
                return m.piece()->state() == PieceState::Captured ||
                       m.piece()->state() == PieceState::Idle;
            }),
        activeMotions_.end()
    );

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

    std::stable_sort(dueMotions.begin(), dueMotions.end(), [](const Motion& a, const Motion& b) noexcept {
        return a.arrivalTime() < b.arrivalTime();
    });

    for (const auto& motion : dueMotions) {
        if (motion.piece()->state() == PieceState::Captured) {
            continue;
        }

        if (!resolver.resolveArrival(motion, currentTimeMs, events, promoteCallback)) {
            activeMotions_.push_back(motion);
        }
    }

    // Continuous smooth movement interpolation is rendered directly by BoardView/Interpolator.
    // We intentionally avoid stepping piece positions cell-by-cell here to prevent first-cell stuttering.

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

std::optional<Motion> RealTimeArbiter::getMotionForPiece(const ConstPiecePtr& piece) const noexcept {
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

bool RealTimeArbiter::isPieceBusy(const PiecePtr& piece, int currentTimeMs) const noexcept {
    if (!piece) return false;
    return isPieceMoving(piece) || 
           piece->state() == PieceState::Airborne || 
           isOnCooldown(piece, currentTimeMs);
}

MoveResult RealTimeArbiter::executeMove(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs) noexcept {
    if (from == to) {
        piece->setState(PieceState::Airborne);
        board_->removePiece(piece);
        startMotion(piece, from, to, currentTimeMs, config_.jumpDurationMs);
        return {true, "jump_started"};
    } else {
        int dr = std::abs(to.row() - from.row());
        int dc = std::abs(to.col() - from.col());
        int distance = std::max(dr, dc);
        int durationMs = distance * config_.msPerCellSpeed;

        piece->setState(PieceState::Moving);
        startMotion(piece, from, to, currentTimeMs, durationMs);
        return {true, "ok"};
    }
}

float RealTimeArbiter::getCooldownProgress(const PiecePtr& piece, int currentTimeMs) const noexcept {
    if (!piece) return 0.0f;
    if (config_.cooldownDurationMs <= 0) {
        return 0.0f;
    }

    int expiresAt = cooldownTracker_.getExpiration(piece->id());
    int remaining = expiresAt - currentTimeMs;
    if (remaining <= 0) return 0.0f;
    
    return static_cast<float>(remaining) / config_.cooldownDurationMs;
}

std::vector<MotionSnapshot> RealTimeArbiter::exportMotions(int currentTimeMs) const {
    std::vector<MotionSnapshot> snapshots;
    snapshots.reserve(activeMotions_.size());

    for (const auto& motion : activeMotions_) {
        if (!motion.piece()) continue;

        int duration = motion.arrivalTime() - motion.startTime();
        int elapsed = currentTimeMs - motion.startTime();
        if (elapsed < 0) elapsed = 0;

        snapshots.push_back({
            motion.piece()->id(),
            motion.from(),
            motion.to(),
            elapsed,
            duration
        });
    }
    return snapshots;
}

void RealTimeArbiter::restoreMotions(
    const std::vector<MotionSnapshot>& snapshots,
    int currentTimeMs,
    const std::unordered_map<std::uint64_t, PiecePtr>& pieceMap) noexcept
{
    activeMotions_.clear();
    activeMotions_.reserve(snapshots.size());

    for (const auto& snap : snapshots) {
        auto it = pieceMap.find(snap.pieceId);
        if (it != pieceMap.end() && it->second) {
            int startTimeMs = currentTimeMs - snap.elapsedMs;
            int durationMs = snap.durationMs;
            activeMotions_.emplace_back(it->second, snap.from, snap.to, startTimeMs, durationMs);
        }
    }
}

std::vector<CooldownSnapshot> RealTimeArbiter::exportCooldowns(int currentTimeMs) const {
    return cooldownTracker_.exportSnapshot(currentTimeMs);
}

void RealTimeArbiter::restoreCooldowns(const std::vector<CooldownSnapshot>& snapshots, int currentTimeMs) noexcept {
    cooldownTracker_.restoreSnapshot(snapshots, currentTimeMs);
}

}  // namespace kungfu