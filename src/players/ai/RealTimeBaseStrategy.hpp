// players/ai/RealTimeBaseStrategy.hpp
#pragma once

#include "players/ai/IAIDecisionStrategy.hpp"
#include "engine/analysis/MoveGenerator.hpp"
#include "engine/common/PieceValues.hpp" 

namespace kungfu {

class RealTimeBaseStrategy : public IAIDecisionStrategy {
protected:
    MoveGenerator moveGenerator_;

    // Immediate move evaluation (based on the value of the captured piece)
    int evaluateImmediateCapture(const view::GameSnapshot& snapshot, const Position& targetPos) const {
        for (const auto& piece : snapshot.pieces) {
            if (piece.logicalPosition == targetPos && piece.state != PieceState::Captured) {
                return PieceValues::getCentipawnValue(piece.type);
            }
        }
        return 0; // normal move
    }

    // Check whether a given square is threatened by the opponent
    bool isSquareThreatened(const view::GameSnapshot& snapshot, const Position& pos, PlayerColor defenderColor) const {
        PlayerColor attackerColor = (defenderColor == PlayerColor::White) ? PlayerColor::Black : PlayerColor::White;
        auto opponentMoves = moveGenerator_.generateForPlayer(snapshot, attackerColor);
        for (const auto& m : opponentMoves) {
            if (m.to == pos) {
                return true;
            }
        }
        return false;
    }
};

} // namespace kungfu