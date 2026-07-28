// core/engine/GameEngine.cpp
#include "engine/core/GameEngine.hpp"
#include "engine/analysis/PositionEvaluator.hpp"
#include "engine/common/GameConfig.hpp"
#include "engine/board/Board.hpp"   
#include "engine/board/Piece.hpp" 
#include <algorithm>
#include <cmath>
#include <unordered_map>  

namespace kungfu {

GameEngine::GameEngine(std::shared_ptr<IBoard> board,
                       std::shared_ptr<RuleEngine> ruleEngine,
                       GameConfig config,
                       std::shared_ptr<IPromotionRule> promotionRule,
                       std::shared_ptr<EventBus> eventBus) noexcept
    : board_(std::move(board))
    , ruleEngine_(std::move(ruleEngine))
    , promotionRule_(std::move(promotionRule))
    , config_(config)
    , arbiter_(board_, config_)
    , eventBus_(std::move(eventBus)) {}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (gameOver_) {
        return {false, "game_over"};
    }

    if (!board_ || !ruleEngine_) {
        return {false, "internal_error"};
    }

    // 1. Locate the piece (on the board or in motion)
    auto sourcePieceOpt = board_->pieceAt(from);
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        sourcePieceOpt = arbiter_.getPieceInTransitAt(from);
    }

    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }
    auto piece = sourcePieceOpt.value();

    // 2. Query physical state through the Arbiter only
    if (arbiter_.isOnCooldown(piece, currentTimeMs_)) {
        if (config_.allowSimultaneousMovement && config_.enablePremoves && from != to) {
            return handlePremoveRegistration(piece, from, to);
        }
        return {false, "piece_on_cooldown"};
    }

    if (arbiter_.isPieceBusy(piece, currentTimeMs_)) {
        if (from == to) {
            return {false, "piece_on_cooldown"};
        }
        return handlePremoveRegistration(piece, from, to);
    }

    // 3. Validate turns and simultaneous legality
    if (!config_.allowSimultaneousMovement) {
        if (piece->color() != currentTurn_) {
            return {false, "not_your_turn"};
        }
        if (arbiter_.hasActiveMotion()) {
            return {false, "motion_in_progress"};
        }
    }

    // 4. Validate movement rules
    if (from != to) {
        auto validation = ruleEngine_->validateMove(from, to);
        if (!validation.isValid) {
            return {false, validation.reason};
        }
    } else {
        if (!config_.allowJumping) {
            return {false, "jumping_disabled"};
        }
        if (piece->state() == PieceState::Captured) {
            return {false, "captured_piece_cannot_jump"};
        }
    }

    // 5. Delegate motion execution to Arbiter
    auto result = arbiter_.executeMove(piece, from, to, currentTimeMs_);

    // 6. Update overall game state
    if (result.isAccepted && !config_.allowSimultaneousMovement) {
        pendingTurnPiece_ = piece;
        advanceTurn();
    }

    return result;
}

std::vector<ActionResult> GameEngine::processActionRequests(const std::vector<ActionRequest>& requests) {
    std::vector<ActionResult> results;
    results.reserve(requests.size());

    for (const auto& request : requests) {
        auto moveResult = requestMove(request.action.from, request.action.to);
        results.emplace_back(request.requestId,
            moveResult.isAccepted ? ActionStatus::Accepted : ActionStatus::Rejected);
    }

    return results;
}

MoveResult GameEngine::handlePremoveRegistration(const PiecePtr& piece, const Position& from, const Position& to) noexcept {
    if (!config_.allowSimultaneousMovement) {
        if (arbiter_.isPieceMoving(piece) || piece->state() == PieceState::Airborne) {
            return {false, "motion_in_progress"};
        }
        return {false, "piece_on_cooldown"};
    }

    if (!config_.enablePremoves) {
        if (arbiter_.isPieceMoving(piece) || piece->state() == PieceState::Airborne) {
            return {false, "motion_in_progress"};
        }
        return {false, "piece_on_cooldown"};
    }

    premoveQueue_.registerOrUpdate(piece, to);
    return {true, "premove_registered"};
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
            if (pendingTurnPiece_ == piece) {
                pendingTurnPiece_ = promoted;
            }
        }
        return promoted;
    };
    auto events = arbiter_.advanceTime(ms, currentTimeMs_, promotionCallback);
    for (const auto& event : events) {
        if (event.capturedKing) {
            gameOver_ = true;
            
            if (eventBus_) {
                GameTransitionEvent endEvent{
                    GameTransitionType::Ended,
                    event.piece ? event.piece->color() : PlayerColor::White
                };
                eventBus_->publish(endEvent);
            }
        }
        
        if (eventBus_) {
            eventBus_->publish(MoveCompletedEvent{event}); 
            if (event.capturedKing) {
                eventBus_->publish(PlaySoundEvent{"game_over"});
            } else if (event.isCapture) {
                eventBus_->publish(PlaySoundEvent{"capture"});
            } else {
                eventBus_->publish(PlaySoundEvent{"move"});
            }
            
            int whiteScore = PositionEvaluator::evaluateBalance(*board_, arbiter_);
            int blackScore = -whiteScore;
            eventBus_->publish(ScoreChangedEvent{whiteScore, blackScore});
        }
    }
    if (config_.enablePremoves && !gameOver_) {
        premoveFailures_.clear();
        premoveQueue_.processReady(
            [this](const PiecePtr& piece) { return arbiter_.isPieceBusy(piece, currentTimeMs_); },
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

int GameEngine::getScore() const noexcept {
    if (!board_) return 0;
    return PositionEvaluator::evaluateBalance(*board_, arbiter_);
}

MoveResult GameEngine::applyServerMove(const Position& from, const Position& to) noexcept {
    if (gameOver_) {
        return {false, "game_over"};
    }
    if (!board_) {
        return {false, "internal_error"};
    }

    // Ignore duplicate server move requests if the piece is already moving to 'to'
    for (const auto& motion : arbiter_.activeMotions()) {
        if (motion.from() == from && motion.to() == to && motion.piece()) {
            return {true, "already_moving"};
        }
    }
    // 1. Search for the piece directly at 'from' on the board grid
    auto sourcePieceOpt = board_->pieceAt(from);

    // 2. Search active motions for a piece animating toward or from 'from'
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        for (const auto& motion : arbiter_.activeMotions()) {
            if ((motion.to() == from || motion.from() == from) && motion.piece()) {
                if (motion.piece()->state() != PieceState::Captured) {
                    sourcePieceOpt = motion.piece();
                    break;
                }
            }
        }
    }

    // 3. Fallback search for in-transit pieces
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        sourcePieceOpt = arbiter_.getPieceInTransitAt(from);
    }

    // 4. Scan all board pieces to find any matching piece that drifted or was marked captured by local desync
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        for (auto& p : board_->pieces()) {
            if (p && p->position() == from) {
                sourcePieceOpt = p;
                break;
            }
        }
    }

    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }

    auto piece = sourcePieceOpt.value();

    // Cancel any active in-flight motion for this piece to avoid animation/hopping conflicts
    arbiter_.cancelMotionForPiece(piece);

    // Force-reset state to Idle if it was temporarily Airborne or Captured locally
    piece->setState(PieceState::Idle);

    // Force-synchronize piece position to 'from' before initiating directional movement
    piece->setPosition(from);
    board_->placePiece(piece, from);

    // Execute the new server move smoothly
    auto result = arbiter_.executeMove(piece, from, to, currentTimeMs_);
    if (result.isAccepted && !config_.allowSimultaneousMovement) {
        advanceTurn();
    }
    return result;
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
    return arbiter_.getPieceInTransitAt(pos).has_value();
}

int GameEngine::getBoardRows() const {
    return board_ ? board_->rows() : 0;
}

int GameEngine::getBoardCols() const {
    return board_ ? board_->cols() : 0;
}

MatchStateSnapshot GameEngine::exportState() const {
    MatchStateSnapshot snapshot;
    snapshot.currentTimeMs = currentTimeMs_;
    snapshot.gameOver = gameOver_;
    snapshot.currentTurn = currentTurn_;

    if (board_) {
        const auto& pieces = board_->pieces();
        snapshot.pieces.reserve(pieces.size());

        for (const auto& piece : pieces) {
            if (!piece) continue;
            PieceStateSnapshot pSnap;
            pSnap.id = piece->id();
            pSnap.type = piece->type();
            pSnap.color = piece->color();
            pSnap.position = piece->position();
            pSnap.state = piece->state();
            pSnap.hasMoved = piece->hasMoved();
            snapshot.pieces.push_back(pSnap);
        }
    }

    snapshot.activeMotions = arbiter_.exportMotions(currentTimeMs_);
    snapshot.activeCooldowns = arbiter_.exportCooldowns(currentTimeMs_);
    snapshot.pendingPremoves = premoveQueue_.exportSnapshot();

    return snapshot;
}

void GameEngine::restoreState(const MatchStateSnapshot& snapshot) {
    currentTimeMs_ = snapshot.currentTimeMs;
    gameOver_ = snapshot.gameOver;
    currentTurn_ = snapshot.currentTurn;

    auto newBoard = std::make_shared<Board>(8, 8);
    std::unordered_map<std::uint64_t, PiecePtr> pieceMap;
    pieceMap.reserve(snapshot.pieces.size());

    for (const auto& pSnap : snapshot.pieces) {
        auto piece = std::make_shared<Piece>(pSnap.type, pSnap.color, pSnap.position);
        piece->setState(pSnap.state);
        if (pSnap.hasMoved) {
            piece->markMoved();
        }
        
        if (pSnap.state != PieceState::Captured) {
            newBoard->placePiece(piece, pSnap.position);
        }
        pieceMap[pSnap.id] = piece;
    }

    board_ = newBoard;
    ruleEngine_ = std::make_shared<RuleEngine>(board_);

    arbiter_ = RealTimeArbiter(board_, config_);
    arbiter_.restoreMotions(snapshot.activeMotions, currentTimeMs_, pieceMap);
    arbiter_.restoreCooldowns(snapshot.activeCooldowns, currentTimeMs_);
    
    premoveQueue_.restoreSnapshot(snapshot.pendingPremoves, pieceMap);
}

}  // namespace kungfu