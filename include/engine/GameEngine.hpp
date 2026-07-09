#pragma once

#include "engine/IGameEngine.hpp"
#include "board/IBoard.hpp"
#include "rules/RuleEngine.hpp"
#include "realtime/RealTimeArbiter.hpp"
#include "engine/GameSnapshot.hpp"

namespace kungfu {

class GameEngine : public IGameEngine {
public:
    GameEngine(std::shared_ptr<IBoard> board, std::shared_ptr<RuleEngine> ruleEngine) noexcept;
        // מימוש ממשק IGameEngine
    MoveResult requestMove(const Position& from, const Position& to) override;
    bool hasPieceAt(const Position& pos) const override;
    int getBoardRows() const override;
    int getBoardCols() const override;

    // התקדמות זמן מדומה
    void wait(int ms) noexcept;
    bool isGameOver() const noexcept { return gameOver_; }
    int getCurrentTimeMs() const noexcept { return currentTimeMs_; }
    
    // מייצר תמונת מצב לקריאה בלבד לטובת הציור
    GameSnapshot getSnapshot(std::optional<Position> selectedCell) const noexcept;
    
private:
    std::shared_ptr<IBoard> board_;
    std::shared_ptr<RuleEngine> ruleEngine_;
    RealTimeArbiter arbiter_;
    int currentTimeMs_ = 0;
    bool gameOver_ = false;
};

}  // namespace kungfu