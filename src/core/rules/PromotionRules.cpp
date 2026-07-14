#include "core/rules/PromotionRules.hpp"

namespace kungfu {

PiecePtr ChessPromotionRule::maybePromote(const PiecePtr& piece, const Position& to, IBoard& board) const {
    if (!piece || piece->type() != PieceType::Pawn) {
        return piece;
    }

    bool reachedLastRow = (piece->color() == PlayerColor::White && to.row() == board.rows() - 1) ||
                          (piece->color() == PlayerColor::Black && to.row() == 0);

    if (!reachedLastRow) {
        return piece;
    }

    auto queen = std::make_shared<Piece>(PieceType::Queen, piece->color(), to);
    board.replacePiece(to, queen);
    return queen;
}

}  // namespace kungfu