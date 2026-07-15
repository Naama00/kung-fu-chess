// core/engine/GameEngine.hpp
#pragma once

#include "core/engine/IGameEngine.hpp"
#include "core/engine/PremoveQueue.hpp"
#include "core/board/IBoard.hpp"
#include "core/rules/RuleEngine.hpp"
#include "core/rules/IPromotionRule.hpp"
#include "core/rules/PromotionRules.hpp"
#include "core/realtime/RealTimeArbiter.hpp"
#include "core/common/GameConfig.hpp"
#include "core/common/ArrivalEvent.hpp"
#include <memory>
#include <vector>
#include <string>

namespace kungfu {

// הצהרה מוקדמת (Forward Declaration) בלבד למשקיף
class IGameObserver;

class GameEngine : public IGameEngine {
public:
    GameEngine(std::shared_ptr<IBoard> board,
               std::shared_ptr<RuleEngine> ruleEngine,
               GameConfig config = GameConfig{},
               std::shared_ptr<IPromotionRule> promotionRule = std::make_shared<ChessPromotionRule>()) noexcept;

    MoveResult requestMove(const Position& from, const Position& to) override;
    bool hasPieceAt(const Position& pos) const override;
    int getBoardRows() const override;
    int getBoardCols() const override;
    std::optional<PlayerColor> getPieceColorAt(const Position& pos) const override;
    void wait(int ms) noexcept;
    bool isGameOver() const noexcept { return gameOver_; }
    int getCurrentTimeMs() const noexcept { return currentTimeMs_; }
    PlayerColor currentTurn() const noexcept { return currentTurn_; }
    std::shared_ptr<const IBoard> getBoard() const noexcept { return board_; }
    const RealTimeArbiter& getArbiter() const noexcept { return arbiter_; }
    const std::vector<MoveResult>& lastPremoveFailures() const noexcept { return premoveFailures_; }
    const PremoveQueue& getPremoveQueue() const noexcept { return premoveQueue_; }

    void addObserver(std::shared_ptr<IGameObserver> observer) noexcept;
    int getScore() const noexcept;

private:
    void advanceTurn() noexcept;
    MoveResult handlePremoveRegistration(const PiecePtr& piece, const Position& from, const Position& to) noexcept;

    std::shared_ptr<IBoard> board_;
    std::shared_ptr<RuleEngine> ruleEngine_;
    std::shared_ptr<IPromotionRule> promotionRule_;
    GameConfig config_;
    RealTimeArbiter arbiter_;
    PremoveQueue premoveQueue_;
    int currentTimeMs_ = 0;
    bool gameOver_ = false;
    PlayerColor currentTurn_ = PlayerColor::White;
    PiecePtr pendingTurnPiece_;   
    std::vector<MoveResult> premoveFailures_;

    std::vector<std::shared_ptr<IGameObserver>> observers_; // רשימת המשקיפים
};

}  // namespace kungfu