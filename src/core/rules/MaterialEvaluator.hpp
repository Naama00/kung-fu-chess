// core/rules/MaterialEvaluator.hpp
#pragma once
#include "core/board/IBoard.hpp"
#include "core/realtime/RealTimeArbiter.hpp"
#include "core/common/Enums.hpp"

namespace kungfu {

class MaterialEvaluator {
public:
    static int getPieceValue(PieceType type) noexcept {
        switch (type) {
            case PieceType::Pawn:   return 1;
            case PieceType::Knight: return 3;
            case PieceType::Bishop: return 3;
            case PieceType::Rook:   return 5;
            case PieceType::Queen:  return 9;
            default:                return 0;
        }
    }

    static int evaluateBalance(const IBoard& board, const RealTimeArbiter& arbiter) noexcept {
        int whiteMaterial = 0;
        int blackMaterial = 0;

        for (const auto& piece : board.pieces()) {
            if (piece && piece->state() != PieceState::Captured) {
                int val = getPieceValue(piece->type());
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
                int val = getPieceValue(piece->type());
                if (piece->color() == PlayerColor::White) {
                    whiteMaterial += val;
                } else {
                    blackMaterial += val;
                }
            }
        }

        return whiteMaterial - blackMaterial;
    }
};

} // namespace kungfu