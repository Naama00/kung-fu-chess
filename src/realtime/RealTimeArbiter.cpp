#include "realtime/RealTimeArbiter.hpp"

namespace kungfu {

RealTimeArbiter::RealTimeArbiter(std::shared_ptr<IBoard> board) noexcept
    : board_(std::move(board)) {}

bool RealTimeArbiter::hasActiveMotion() const noexcept {
    return !activeMotions_.empty();
}

void RealTimeArbiter::startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept {
    activeMotions_.emplace_back(std::move(piece), from, to, currentTimeMs, durationMs);
}

std::vector<ArrivalEvent> RealTimeArbiter::advanceTime(int ms, int& currentTimeMs) noexcept {
    std::vector<ArrivalEvent> events;
    currentTimeMs += ms;

    for (auto it = activeMotions_.begin(); it != activeMotions_.end(); ) {
        if (currentTimeMs >= it->arrivalTime()) {
            Position from = it->from();
            Position to = it->to();
            auto piece = it->piece();

            auto targetPieceOpt = board_->pieceAt(to);

            // --- לוגיקת ביטול מהלך (Cancellation) ---
            // אם ברגע ההגעה המדויק, יעד התנועה נתפס על ידי כלי ידידותי
            if (targetPieceOpt.has_value() && targetPieceOpt.value() && targetPieceOpt.value()->color() == piece->color()) {
                piece->setState(PieceState::Idle); // החזרת הכלי למצב רגיל במשבצת המקור שלו
                events.push_back({from, from, piece, false, true}); // אירוע ביטול
                it = activeMotions_.erase(it); // הסרת התנועה מהוקטור
                continue;
            }

            // הגעה רגילה / אכילת כלי עוין
            bool capturedKing = false;
            if (targetPieceOpt.has_value() && targetPieceOpt.value()) {
                auto targetPiece = targetPieceOpt.value();
                if (targetPiece->type() == PieceType::King) {
                    capturedKing = true;
                }
                targetPiece->setState(PieceState::Captured);
                cooldowns_.erase(targetPiece.get());
                board_->removePiece(to);
            }

            board_->movePiece(from, to);
            piece->setState(PieceState::Idle);

            // רישום זמן צינון
            cooldowns_[piece.get()] = currentTimeMs + 2000;

            events.push_back({from, to, piece, capturedKing, false});
            it = activeMotions_.erase(it);
        } else {
            ++it;
        }
    }

    return events;
}

bool RealTimeArbiter::isOnCooldown(const PiecePtr& piece, int currentTimeMs) const noexcept {
    if (!piece) return false;
    auto it = cooldowns_.find(piece.get());
    if (it != cooldowns_.end()) {
        return currentTimeMs < it->second;
    }
    return false;
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