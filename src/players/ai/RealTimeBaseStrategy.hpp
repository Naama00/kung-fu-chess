// players/ai/RealTimeBaseStrategy.hpp
#pragma once

#include "players/ai/IAIDecisionStrategy.hpp"
#include "engine/analysis/MoveGenerator.hpp"

namespace kungfu {

class RealTimeBaseStrategy : public IAIDecisionStrategy {
protected:
    MoveGenerator moveGenerator_;

    // הערכת המהלך המיידי (מבוסס על שווי הכלי הנלכד)
    int evaluateImmediateCapture(const view::GameSnapshot& snapshot, const Position& targetPos) const {
        for (const auto& piece : snapshot.pieces) {
            if (piece.logicalPosition == targetPos && piece.state != PieceState::Captured) {
                return getPieceValue(piece.type);
            }
        }
        return 0; // תנועה רגילה
    }

    // בדיקה האם משבצת מסוימת מאוימת על ידי היריב
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

    int getPieceValue(PieceType type) const {
        switch (type) {
            case PieceType::Queen:  return 900;
            case PieceType::Rook:   return 500;
            case PieceType::Bishop: return 300;
            case PieceType::Knight: return 300;
            case PieceType::Pawn:   return 100;
            case PieceType::King:   return 10000;
            default:                return 0;
        }
    }
};

} // namespace kungfu