// players/ai/ClassicMinimaxStrategy.hpp
#pragma once

#include "players/ai/IAIDecisionStrategy.hpp"
#include "engine/analysis/MoveGenerator.hpp"
#include <random>

namespace kungfu {

class ClassicMinimaxStrategy : public IAIDecisionStrategy {
private:
    MoveGenerator moveGenerator_;
    int searchDepth_;
    mutable std::mt19937 rng_;

    struct PieceUndo {
        std::size_t pieceIndex;
        Position originalPos;
        PieceState originalState;
        bool originalHasMoved;
        std::size_t capturedIndex;
        bool originalIsGameOver;
    };

    PieceUndo applyMove(view::GameSnapshot& snapshot, const Position& from, const Position& to, PlayerColor color) const;
    void undoMove(view::GameSnapshot& snapshot, const PieceUndo& undo) const;
    int minimax(view::GameSnapshot& snapshot, int depth, int alpha, int beta, bool maximizingPlayer, PlayerColor aiColor) const;

public:
    explicit ClassicMinimaxStrategy(int searchDepth = 2, std::mt19937::result_type seed = 0xC0FFEEu);
    ~ClassicMinimaxStrategy() override = default;

    std::vector<ActionRequest> computeActions(const view::GameSnapshot& snapshot, PlayerColor aiColor) override;
};

} // namespace kungfu