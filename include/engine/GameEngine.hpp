#pragma once

#include "engine/IGameEngine.hpp"
#include "engine/PremoveQueue.hpp"
#include "board/IBoard.hpp"
#include "rules/RuleEngine.hpp"
#include "rules/IPromotionRule.hpp"
#include "rules/PromotionRules.hpp"
#include "realtime/RealTimeArbiter.hpp"
#include "common/GameConfig.hpp"
#include <memory>

namespace kungfu {

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

private:
    void advanceTurn() noexcept;
    bool isPieceBusy(const PiecePtr& piece) const noexcept;

    MoveResult handlePremoveRegistration(const PiecePtr& piece, const Position& from, const Position& to) noexcept;
    MoveResult handleJumpRequest(const PiecePtr& piece, const Position& pos) noexcept;
    MoveResult handleStandardMove(const PiecePtr& piece, const Position& from, const Position& to) noexcept;

    std::shared_ptr<IBoard> board_;
    std::shared_ptr<RuleEngine> ruleEngine_;
    std::shared_ptr<IPromotionRule> promotionRule_;
    GameConfig config_;
    RealTimeArbiter arbiter_;
    PremoveQueue premoveQueue_;
    int currentTimeMs_ = 0;
    bool gameOver_ = false;
    PlayerColor currentTurn_ = PlayerColor::White;
    std::vector<MoveResult> premoveFailures_;
};

}  // namespace kungfu