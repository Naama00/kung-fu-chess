#pragma once

#include "engine/common/Position.hpp"
#include "engine/board/Piece.hpp"

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