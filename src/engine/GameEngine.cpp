#include "engine/GameEngine.hpp"
#include "engine/GameSnapshot.hpp"
#include <algorithm>
#include <cmath>

namespace kungfu {

GameEngine::GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<RuleEngine> ruleEngine) noexcept
    : board_(std::move(board))
    , ruleEngine_(std::move(ruleEngine))
    , arbiter_(board_) {}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (gameOver_) {
        return {false, "game_over"};
    }

    if (arbiter_.hasActiveMotion()) {
        return {false, "motion_in_progress"};
    }

    if (!board_ || !ruleEngine_) {
        return {false, "internal_error"};
    }

    auto validation = ruleEngine_->validateMove(from, to);
    if (!validation.isValid) {
        return {false, validation.reason};
    }

    auto sourcePieceOpt = board_->pieceAt(from);
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }

    auto piece = sourcePieceOpt.value();

    if (arbiter_.isPieceMoving(piece)) {
        return {false, "motion_in_progress"};
    }

    if (arbiter_.isOnCooldown(piece, currentTimeMs_)) {
        return {false, "piece_on_cooldown"};
    }

    int dr = std::abs(to.row() - from.row());
    int dc = std::abs(to.col() - from.col());
    int distance = std::max(dr, dc);
    int durationMs = distance * 1000;

    piece->setState(PieceState::Moving);
    arbiter_.startMotion(piece, from, to, currentTimeMs_, durationMs);

    return {true, "ok"};
}

bool GameEngine::hasPieceAt(const Position& pos) const {
    if (!board_) {
        return false;
    }
    auto pieceOpt = board_->pieceAt(pos);
    return pieceOpt.has_value() && pieceOpt.value() != nullptr;
}

int GameEngine::getBoardRows() const {
    return board_ ? board_->rows() : 0;
}

int GameEngine::getBoardCols() const {
    return board_ ? board_->cols() : 0;
}

void GameEngine::wait(int ms) noexcept {
    if (ms <= 0 || !board_) {
        return;
    }

    auto events = arbiter_.advanceTime(ms, currentTimeMs_);
    for (const auto& event : events) {
        if (event.capturedKing) {
            gameOver_ = true;
        }
    }
}

GameSnapshot GameEngine::getSnapshot(std::optional<Position> selectedCell) const noexcept {
    GameSnapshot snap;
    snap.boardCols = getBoardCols();
    snap.boardRows = getBoardRows();
    snap.isGameOver = gameOver_;
    snap.selectedCell = selectedCell;

    if (!board_) {
        return snap;
    }

    for (const auto& piece : board_->pieces()) {
        if (!piece || piece->state() == PieceState::Captured) {
            continue;
        }

        PieceSnapshot pSnap;
        pSnap.type = piece->type();
        pSnap.color = piece->color();
        pSnap.logicalPosition = piece->position();
        pSnap.state = piece->state();

        float currentX = piece->position().col() * 100.0f;
        float currentY = piece->position().row() * 100.0f;

        pSnap.pixelX = currentX;
        pSnap.pixelY = currentY;

        if (piece->state() == PieceState::Moving) {
            auto motionOpt = arbiter_.getMotionForPiece(piece);
            if (motionOpt.has_value()) {
                const auto& motion = motionOpt.value();
                float startX = motion.from().col() * 100.0f;
                float startY = motion.from().row() * 100.0f;
                float endX = motion.to().col() * 100.0f;
                float endY = motion.to().row() * 100.0f;

                int duration = motion.arrivalTime() - motion.startTime();
                if (duration > 0) {
                    int elapsed = currentTimeMs_ - motion.startTime();
                    float t = static_cast<float>(elapsed) / static_cast<float>(duration);

                    t = std::max(0.0f, std::min(t, 1.0f));

                    pSnap.pixelX = startX + t * (endX - startX);
                    pSnap.pixelY = startY + t * (endY - startY);
                }
            }
        }

        snap.pieces.push_back(pSnap);
    }

    return snap;
}

}  // namespace kungfu
