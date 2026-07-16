#pragma once

#include "core/common/Position.hpp"
#include "core/board/Piece.hpp"

namespace kungfu {

struct ArrivalEvent {
    Position from;
    Position to;
    PiecePtr piece;
    bool capturedKing;
    bool cancelled; 
    bool isCapture = false;
};

} // namespace kungfu