// core/engine/GameEngine.cpp
#include "engine/core/GameEngine.hpp"
#include "engine/events/IGameObserver.hpp"
#include "engine/analysis/PositionEvaluator.hpp"
#include "engine/common/GameConfig.hpp"
#include <algorithm>
#include <cmath>

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

    // 2. Query physical state through the Arbiter only (DIP)
if (arbiter_.isOnCooldown(piece, currentTimeMs_)) {
    // If the game is in simultaneous mode and premoves are enabled, allow a premove to be registered even while the piece is in cooldown
    if (config_.allowSimultaneousMovement && config_.enablePremoves && from != to) {
        return handlePremoveRegistration(piece, from, to);
    }
    return {false, "piece_on_cooldown"};
}

    if (arbiter_.isPieceBusy(piece, currentTimeMs_)) {
        // Prevent registering a premove for a self-jump (to avoid automatic jumping after cooldown)
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

    // 4. Validate movement rules against the geometric RuleEngine (normal move)
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

    // 5. Delegate the full motion execution to the Arbiter (SRP principle)
    auto result = arbiter_.executeMove(piece, from, to, currentTimeMs_);

    // 6. Update the overall game state (turn change) as needed
    if (result.isAccepted && !config_.allowSimultaneousMovement) {
        pendingTurnPiece_ = piece; // Save for resetting the turn after cooldown
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
    // In turn-based mode (non-simultaneous), premoves are not supported regardless of the enablePremoves flag
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
            
            // Publish the game-over event on the Bus
            if (eventBus_) {
                GameTransitionEvent endEvent{
                    GameTransitionType::Ended,
                    event.piece ? event.piece->color() : PlayerColor::White
                };
                eventBus_->publish(endEvent);
            }
        }
        
        // Publish the generic event to the BUS
        if (eventBus_) {
            // 1. Publish a move-completion event
            eventBus_->publish(MoveCompletedEvent{event}); 
            // 2. Automatically publish an appropriate sound event based on the action type
            if (event.capturedKing) {
                eventBus_->publish(PlaySoundEvent{"game_over"});
            } else if (event.isCapture) {
                eventBus_->publish(PlaySoundEvent{"capture"});
            } else {
                eventBus_->publish(PlaySoundEvent{"move"});
            }
            
            // 3. Calculate and publish the updated score change
            int whiteScore = PositionEvaluator::evaluateBalance(*board_, arbiter_); // or simple material-based scoring logic
            int blackScore = -whiteScore; // or the existing piece-scoring logic in ChessGameScreen
            eventBus_->publish(ScoreChangedEvent{whiteScore, blackScore});
        }

        // Keep the existing observer mechanism so as not to break prior dependencies in one shot:
        for (auto& observer : observers_) {
            observer->onMoveCompleted(event, currentTimeMs_);
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

void GameEngine::addObserver(std::shared_ptr<IGameObserver> observer) noexcept {
    if (observer) {
        observers_.push_back(observer);
    }
}

int GameEngine::getScore() const noexcept {
    if (!board_) return 0;
    return PositionEvaluator::evaluateBalance(*board_, arbiter_);
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

}  // namespace kungfu