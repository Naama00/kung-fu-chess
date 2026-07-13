#pragma once

#include "common/Position.hpp"
#include "board/Piece.hpp"

namespace kungfu {

struct ArrivalEvent {
    Position from;
    Position to;
    PiecePtr piece;
    bool capturedKing;
    bool cancelled; 
};

} // namespace kungfu