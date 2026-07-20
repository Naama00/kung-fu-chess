// engine/analysis/PositionEvaluator.cpp
#include "engine/analysis/PositionEvaluator.hpp"
#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/board/IBoard.hpp"
#include "engine/realtime/RealTimeArbiter.hpp"
#include "engine/analysis/MoveGenerator.hpp"
#include "engine/common/PieceValues.hpp" 
#include <vector>

namespace kungfu {

int PositionEvaluator::getPieceValue(PieceType type) noexcept {
    return PieceValues::getCentipawnValue(type);
}

int PositionEvaluator::evaluate(const view::GameSnapshot& snapshot, PlayerColor evaluatingPlayer) const noexcept {
    int myMaterial = 0;
    int enemyMaterial = 0;
    int myThreatPenalty = 0;
    int enemyThreatPenalty = 0;

    MoveGenerator generator;
    
    // Generate the moves only once
    auto whiteMoves = generator.generateForPlayer(snapshot, PlayerColor::White);
    auto blackMoves = generator.generateForPlayer(snapshot, PlayerColor::Black);

    auto isWhiteSquareThreatened = [&](const Position& pos) {
        for (const auto& m : blackMoves) {
            if (m.to == pos) return true;
        }
        return false;
    };

    auto isBlackSquareThreatened = [&](const Position& pos) {
        for (const auto& m : whiteMoves) {
            if (m.to == pos) return true;
        }
        return false;
    };

    for (const auto& piece : snapshot.pieces) {
        if (piece.state == PieceState::Captured) {
            continue;
        }

        int val = getPieceValue(piece.type);
        
        bool isThreatened = (piece.color == PlayerColor::White) 
                            ? isWhiteSquareThreatened(piece.logicalPosition)
                            : isBlackSquareThreatened(piece.logicalPosition);

        if (piece.color == evaluatingPlayer) {
            myMaterial += val;
            if (isThreatened) {
                myThreatPenalty += static_cast<int>(0.3f * val);
            }
        } else {
            enemyMaterial += val;
            if (isThreatened) {
                enemyThreatPenalty += static_cast<int>(0.3f * val);
            }
        }
    }

    return (myMaterial - enemyMaterial) - (myThreatPenalty - enemyThreatPenalty);
}

int PositionEvaluator::evaluateBalance(const IBoard& board, const RealTimeArbiter& arbiter) noexcept {
    int whiteMaterial = 0;
    int blackMaterial = 0;

    auto getOldValue = [](PieceType type) {
        switch (type) {
            case PieceType::Pawn:   return 1;
            case PieceType::Knight: return 3;
            case PieceType::Bishop: return 3;
            case PieceType::Rook:   return 5;
            case PieceType::Queen:  return 9;
            default:                return 0;
        }
    };

    for (const auto& piece : board.pieces()) {
        if (piece && piece->state() != PieceState::Captured) {
            int val = getOldValue(piece->type());
            if (piece->color() == PlayerColor::White) {
                whiteMaterial += val;
            } else {
                blackMaterial += val;
            }
        }
    }

    for (const auto& motion : arbiter.activeMotions()) {
        auto piece = motion.piece();
        if (piece && piece->state() == PieceState::Airborne) {
            int val = getOldValue(piece->type());
            if (piece->color() == PlayerColor::White) {
                whiteMaterial += val;
            } else {
                blackMaterial += val;
            }
        }
    }

    return whiteMaterial - blackMaterial;
}

} // namespace kungfu