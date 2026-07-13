// include/board/Board.hpp
#pragma once

#include <vector>
#include "board/IBoard.hpp"
#include "common/Enums.hpp"
#include "common/Position.hpp"

namespace kungfu {

class Board : public IBoard {
public:
    Board();
    Board(int rows, int cols); 

    int rows() const override { return rows_; }
    int cols() const override { return cols_; }
    
    std::vector<PiecePtr> pieces() override;
    std::vector<std::shared_ptr<const Piece>> pieces() const override;

    std::optional<PiecePtr> pieceAt(const Position& position) override;
    std::optional<std::shared_ptr<const Piece>> pieceAt(const Position& position) const override;
    
    bool placePiece(const PiecePtr& piece, const Position& position) override;
    bool removePiece(const Position& position) override;
    bool movePiece(const Position& from, const Position& to) override;
    bool replacePiece(const Position& position, const PiecePtr& newPiece) override;

private:
    std::vector<PiecePtr> pieces_;
    int rows_ = 8;
    int cols_ = 8;
};

}  // namespace kungfu