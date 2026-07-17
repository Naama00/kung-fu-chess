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
                       std::shared_ptr<IPromotionRule> promotionRule) noexcept
    : board_(std::move(board))
    , ruleEngine_(std::move(ruleEngine))
    , promotionRule_(std::move(promotionRule))
    , config_(config)
    , arbiter_(board_, config_) {}

MoveResult GameEngine::requestMove(const Position& from, const Position& to) {
    if (gameOver_) {
        return {false, "game_over"};
    }

    if (!board_ || !ruleEngine_) {
        return {false, "internal_error"};
    }

    // 1. איתור הכלי (על הלוח או בתנועה)
    auto sourcePieceOpt = board_->pieceAt(from);
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        sourcePieceOpt = arbiter_.getPieceInTransitAt(from);
    }

    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }
    auto piece = sourcePieceOpt.value();

    // 2. שאילתת מצב פיזיקלי דרך ה-Arbiter בלבד (DIP)
if (arbiter_.isOnCooldown(piece, currentTimeMs_)) {
    // אם המשחק במצב סימולטני ופרה-מובס מאופשרים, נאפשר לרשום פרה-מוב גם בזמן שהכלי בצינון
    if (config_.allowSimultaneousMovement && config_.enablePremoves && from != to) {
        return handlePremoveRegistration(piece, from, to);
    }
    return {false, "piece_on_cooldown"};
}

    if (arbiter_.isPieceBusy(piece, currentTimeMs_)) {
        // מניעת רישום פרה-מוב של קפיצה עצמית (מניעת קפיצה אוטומטית לאחר צינון)
        if (from == to) {
            return {false, "piece_on_cooldown"};
        }
        return handlePremoveRegistration(piece, from, to);
    }

    // 3. אימות תורות וחוקיות סימולטנית
    if (!config_.allowSimultaneousMovement) {
        if (piece->color() != currentTurn_) {
            return {false, "not_your_turn"};
        }
        if (arbiter_.hasActiveMotion()) {
            return {false, "motion_in_progress"};
        }
    }

    // 4. אימות חוקי תנועה מול ה-RuleEngine הגיאומטרי (מהלך רגיל)
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

    // 5. האצלת ביצוע התנועה המלאה ל-Arbiter (עקרון ה-SRP)
    auto result = arbiter_.executeMove(piece, from, to, currentTimeMs_);

    // 6. עדכון מצב משחק עליון (החלפת תורות) במידת הצורך
    if (result.isAccepted && !config_.allowSimultaneousMovement) {
        pendingTurnPiece_ = piece; // שמירה לצורך איפוס תור אחרי צינון
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
    // במצב תורות (לא-סימולטני), פרה-מוב אינו נתמך ללא קשר לדגל enablePremoves
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
        }
        
        // הפצת האירועים לכל המשקיפים הרשומים
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