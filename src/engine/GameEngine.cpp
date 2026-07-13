#include "engine/GameEngine.hpp"
#include "common/GameConfig.hpp"
#include <algorithm>
#include <cmath>

namespace kungfu {

GameEngine::GameEngine(std::shared_ptr<IBoard> board,
                       std::shared_ptr<RuleEngine> ruleEngine,
                       GameConfig config,
                       std::shared_ptr<IPromotionRule> promotionRule) noexcept
    : board_(std::move(board))
    , ruleEngine_(std::move(ruleEngine))
    , promotionRule_(std::move(promotionRule))
    , config_(config)
    , arbiter_(board_, config_) {}

bool GameEngine::isPieceBusy(const PiecePtr& piece) const noexcept {
    return arbiter_.isPieceMoving(piece) ||
           piece->state() == PieceState::Airborne ||
           arbiter_.isOnCooldown(piece, currentTimeMs_);
}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (gameOver_) {
        return {false, "game_over"};
    }

    if (!board_ || !ruleEngine_) {
        return {false, "internal_error"};
    }

    auto sourcePieceOpt = board_->pieceAt(from);
    
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        sourcePieceOpt = arbiter_.getPieceInTransitAt(from);
    }

    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }
    auto piece = sourcePieceOpt.value();

    if (!config_.allowSimultaneousMovement) {
        if (piece->color() != currentTurn_) {
            return {false, "not_your_turn"};
        }
        if (arbiter_.hasActiveMotion()) {
            return {false, "motion_in_progress"};
        }
    }

    if (isPieceBusy(piece)) {
        if (!config_.allowSimultaneousMovement) {
            return {false, "piece_busy"};
        }
        return handlePremoveRegistration(piece, from, to);
    }

    if (from == to) {
        return handleJumpRequest(piece, from);
    }

    return handleStandardMove(piece, from, to);
}

MoveResult GameEngine::handlePremoveRegistration(const PiecePtr& piece, const Position& from, const Position& to) noexcept {
    if (!config_.enablePremoves) {
        if (arbiter_.isPieceMoving(piece) || piece->state() == PieceState::Airborne) {
            return {false, "motion_in_progress"};
        }
        return {false, "piece_on_cooldown"};
    }

    premoveQueue_.registerOrUpdate(piece, to);
    return {true, "premove_registered"};
}

MoveResult GameEngine::handleJumpRequest(const PiecePtr& piece, const Position& pos) noexcept {
    if (piece->state() == PieceState::Captured) {
        return {false, "captured_piece_cannot_jump"};
    }

    piece->setState(PieceState::Airborne);
    arbiter_.startMotion(piece, pos, pos, currentTimeMs_, config_.jumpDurationMs);

    if (!config_.allowSimultaneousMovement) {
        advanceTurn();
    }

    return {true, "jump_started"};
}

MoveResult GameEngine::handleStandardMove(const PiecePtr& piece, const Position& from, const Position& to) noexcept {
    auto validation = ruleEngine_->validateMove(from, to);
    if (!validation.isValid) {
        return {false, validation.reason};
    }

    int dr = std::abs(to.row() - from.row());
    int dc = std::abs(to.col() - from.col());
    int distance = std::max(dr, dc);
    int durationMs = distance * config_.msPerCellSpeed;

    piece->setState(PieceState::Moving);
    arbiter_.startMotion(piece, from, to, currentTimeMs_, durationMs);

    if (!config_.allowSimultaneousMovement) {
        advanceTurn();
    }

    return {true, "ok"};
}

void GameEngine::advanceTurn() noexcept {
    currentTurn_ = (currentTurn_ == PlayerColor::White) ? PlayerColor::Black : PlayerColor::White;
}

void GameEngine::wait(int ms) noexcept {
    if (ms <= 0 || !board_) {
        return;
    }
    auto promotionCallback = [this](const PiecePtr& piece, const Position& to) -> PiecePtr {
        if (!promotionRule_) {
            return piece;
        }
        auto promoted = promotionRule_->maybePromote(piece, to, *board_);
        if (promoted != piece) {
            premoveQueue_.replacePiece(piece, promoted);
        }
        return promoted;
    };
    auto events = arbiter_.advanceTime(ms, currentTimeMs_, promotionCallback);
    for (const auto& event : events) {
        if (event.capturedKing) {
            gameOver_ = true;
        }
    }

    if (config_.enablePremoves && !gameOver_) {
        premoveFailures_.clear();
        premoveQueue_.processReady(
            [this](const PiecePtr& piece) { return isPieceBusy(piece); },
            [this](const Position& from, const Position& to) -> MoveResult {
                auto result = requestMove(from, to);
                if (!result.isAccepted) {
                    premoveFailures_.push_back(result);
                }
                return result;
            }
        );
    }
}

std::optional<PlayerColor> GameEngine::getPieceColorAt(const Position& pos) const {
    if (!board_) {
        return std::nullopt;
    }
    auto pieceOpt = board_->pieceAt(pos);
    if (pieceOpt.has_value() && pieceOpt.value()) {
        return pieceOpt.value()->color();
    }
    auto transitOpt = arbiter_.getPieceInTransitAt(pos);
    if (transitOpt.has_value() && transitOpt.value()) {
        return transitOpt.value()->color();
    }
    return std::nullopt;
}

bool GameEngine::hasPieceAt(const Position& pos) const {
    if (!board_) {
        return false;
    }
    auto pieceOpt = board_->pieceAt(pos);
    if (pieceOpt.has_value() && pieceOpt.value() != nullptr) {
        return true;
    }
    // תיקון באג 4: תמיכה בזיהוי כלים בתנועה על מנת לאפשר לחיצות ובחירה מהבקר
    return arbiter_.getPieceInTransitAt(pos).has_value();
}

int GameEngine::getBoardRows() const {
    return board_ ? board_->rows() : 0;
}

int GameEngine::getBoardCols() const {
    return board_ ? board_->cols() : 0;
}

}  // namespace kungfu