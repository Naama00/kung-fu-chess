#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "common/Position.hpp"
#include "board/Piece.hpp"

namespace kungfu {

class IBoard {
public:
    virtual ~IBoard() = default;

    virtual int rows() const = 0;
    virtual int cols() const = 0;
    
    // הפרדה בין גישת כתיבה לגישת קריאה בלבד (Const-Correctness)
    virtual std::vector<PiecePtr> pieces() = 0;
    virtual std::vector<std::shared_ptr<const Piece>> pieces() const = 0;

    virtual std::optional<PiecePtr> pieceAt(const Position& position) = 0;
    virtual std::optional<std::shared_ptr<const Piece>> pieceAt(const Position& position) const = 0;
    
    virtual bool placePiece(const PiecePtr& piece, const Position& position) = 0;
    virtual bool removePiece(const Position& position) = 0;
    virtual bool movePiece(const Position& from, const Position& to) = 0;
    virtual bool removePiece(const PiecePtr& piece) = 0;
    virtual bool replacePiece(const Position& position, const PiecePtr& newPiece) = 0;
};

using BoardPtr = std::shared_ptr<IBoard>;

}  // namespace kungfu