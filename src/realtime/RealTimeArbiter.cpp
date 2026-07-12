#include "realtime/RealTimeArbiter.hpp"
#include "common/GameConfig.hpp"
#include <algorithm>

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board, GameConfig config) noexcept
    : board_(std::move(board)), config_(std::move(config)) {}

bool RealTimeArbiter::hasActiveMotion() const noexcept {
    return !activeMotions_.empty();
}

void RealTimeArbiter::startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept {
    activeMotions_.emplace_back(std::move(piece), from, to, currentTimeMs, durationMs);
}

std::vector<ArrivalEvent> RealTimeArbiter::advanceTime(int ms, int& currentTimeMs, PromotionHandler promoteCallback) noexcept {
    std::vector<ArrivalEvent> events;
    currentTimeMs += ms;

    handleMidRouteCollisions(events);

    for (auto it = activeMotions_.begin(); it != activeMotions_.end(); ) {
        if (currentTimeMs >= it->arrivalTime()) {
            if (processSingleArrival(it, currentTimeMs, events, promoteCallback)) {
                it = activeMotions_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    return events;
}

void RealTimeArbiter::handleMidRouteCollisions(std::vector<ArrivalEvent>& events) noexcept {
    if (!config_.allowSimultaneousMovement || activeMotions_.size() < 2) {
        return;
    }

    for (size_t i = 0; i < activeMotions_.size(); ++i) {
        for (size_t j = i + 1; j < activeMotions_.size(); ++j) {
            auto& m1 = activeMotions_[i];
            auto& m2 = activeMotions_[j];

            if (m1.piece()->color() != m2.piece()->color()) {
                // פרשים אינם יכולים להתנגש – הם קופצים מעל הלוח
                if (m1.piece()->type() == PieceType::Knight || m2.piece()->type() == PieceType::Knight) {
                    continue;
                }

                if (m1.from() == m2.to() && m1.to() == m2.from()) {
                    auto& winner = (m1.startTime() <= m2.startTime()) ? m1 : m2;
                    auto& loser = (m1.startTime() <= m2.startTime()) ? m2 : m1;
                    (void)winner;

                    loser.piece()->setState(PieceState::Captured);
                    cooldownTracker_.clear(loser.piece()->id());
                    board_->removePiece(loser.from());

                    events.push_back({loser.from(), loser.from(), loser.piece(), false, true});
                }
            }
        }
    }

    activeMotions_.erase(
        std::remove_if(activeMotions_.begin(), activeMotions_.end(),
            [](const Motion& m) { return m.piece()->state() == PieceState::Captured; }),
        activeMotions_.end()
    );
}

bool RealTimeArbiter::processSingleArrival(
    std::vector<Motion>::iterator it,
    int currentTimeMs,
    std::vector<ArrivalEvent>& events,
    const PromotionHandler& promoteCallback
) noexcept {
    Position from = it->from();
    Position to = it->to();
    auto piece = it->piece();

    if (from == to && piece->state() == PieceState::Airborne) {
        piece->setState(PieceState::Idle);
        cooldownTracker_.setCooldown(piece->id(), currentTimeMs + config_.cooldownDurationMs);
        events.push_back({from, to, piece, false, false});
        return true;
    }

    auto targetPieceOpt = board_->pieceAt(to);

    if (targetPieceOpt.has_value() && targetPieceOpt.value() && targetPieceOpt.value()->state() == PieceState::Airborne) {
        auto airbornePiece = targetPieceOpt.value();

        if (airbornePiece->color() != piece->color()) {
            // כלי יריב באוויר ביעד – הגיע ראשון ועוצר את הכלי הנוכחי
            piece->setState(PieceState::Captured);
            cooldownTracker_.clear(piece->id());
            board_->removePiece(from);

            events.push_back({from, from, piece, false, true});
            return true;
        } else {
            // כלי ידידותי באוויר ביעד
            if (piece->type() == PieceType::Knight) {
                // פרש תמיד תופס כל כלי ביעד – גם ידידותי
                airbornePiece->setState(PieceState::Captured);
                cooldownTracker_.clear(airbornePiece->id());
                board_->removePiece(to);
                // ממשיך לנחיתה רגילה להלן
            } else {
                // כלי לא-פרש נעצר כשמשבצת היעד תפוסה על ידי ידיד
                piece->setState(PieceState::Idle);
                events.push_back({from, from, piece, false, true});
                return true;
            }
        }
    }

    if (targetPieceOpt.has_value() && targetPieceOpt.value() && targetPieceOpt.value()->color() == piece->color()) {
        if (piece->type() == PieceType::Knight) {
            // פרש תופס כלים ידידותיים על הלוח – הדרך היחידה לחסל כלי של עצמך
            auto targetPiece = targetPieceOpt.value();
            targetPiece->setState(PieceState::Captured);
            cooldownTracker_.clear(targetPiece->id());
            board_->removePiece(to);
            // ממשיך לנחיתה רגילה להלן
        } else {
            // כלי לא-פרש נעצר על ידי כלי ידידותי
            piece->setState(PieceState::Idle);
            events.push_back({from, from, piece, false, true});
            return true;
        }
    }

    bool capturedKing = false;
    if (targetPieceOpt.has_value() && targetPieceOpt.value()) {
        auto targetPiece = targetPieceOpt.value();
        if (targetPiece->type() == PieceType::King) {
            capturedKing = true;
        }
        targetPiece->setState(PieceState::Captured);
        cooldownTracker_.clear(targetPiece->id());
        board_->removePiece(to);
    }

    board_->movePiece(from, to);
    piece->setState(PieceState::Idle);
    piece->markMoved();

    PiecePtr finalPiece = piece;
    if (promoteCallback) {
        finalPiece = promoteCallback(piece, to);
    }
    if (finalPiece != piece) {
        cooldownTracker_.clear(piece->id());
    }
    cooldownTracker_.setCooldown(finalPiece->id(), currentTimeMs + config_.cooldownDurationMs);
    events.push_back({from, to, finalPiece, capturedKing, false});

    return true;
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

}  // namespace kungfu