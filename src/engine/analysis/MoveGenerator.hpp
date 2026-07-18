
// תפקיד: המימוש הקונקרטי של הממשק. הוא בונה באופן דינמי אובייקט לוח (Board)
// מתוך תמונת מצב (GameSnapshot) ומשתמש ב-RuleEngine הגיאומטרי כדי לחשב אילו מהלכים חוקיים.
#pragma once

#include <vector>
#include "engine/analysis/IMoveGenerator.hpp"
#include "engine/board/Board.hpp"
#include "engine/common/Position.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace kungfu {

class MoveGenerator : public IMoveGenerator {
public:
    std::vector<MoveCandidate> generateForPiece(
        const view::GameSnapshot& snapshot,
        const Position& piecePosition) const override;

    std::vector<MoveCandidate> generateForPlayer(
        const view::GameSnapshot& snapshot,
        PlayerColor playerColor) const override;

private:
    std::shared_ptr<Board> buildBoardFromSnapshot(const view::GameSnapshot& snapshot) const;
};

} // namespace kungfu
