#pragma once

#include <vector>
#include "board/IBoard.hpp"
#include "common/Enums.hpp"
#include "common/Position.hpp"
#include "pieces/Piece.hpp"

namespace kungfu {

class Board : public IBoard {
public:
    Board();
    Board(int rows, int cols); // בנאי חדש לתמיכה בממדים דינמיים

    int rows() const override { return rows_; }
    int cols() const override { return cols_; }
    std::vector<PiecePtr> pieces() const override;

    std::optional<PiecePtr> pieceAt(const Position& position) const override;
    bool placePiece(const PiecePtr& piece, const Position& position) override;
    bool removePiece(const Position& position) override;
    bool movePiece(const Position& from, const Position& to) override;
    bool replacePiece(const Position& position, const PiecePtr& newPiece) override;

private:
    std::vector<PiecePtr> pieces_;
    int rows_ = 8; // ברירת מחדל 8
    int cols_ = 8; // ברירת מחדל 8
};

}  // namespace kungfu
