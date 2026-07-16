#pragma once

#include <memory>
#include "engine/common/Position.hpp"
#include "engine/board/Piece.hpp"

namespace kungfu {

class Motion {
public:
    Motion(PiecePtr piece, const Position& from, const Position& to, int startTimeMs, int durationMs) noexcept;

    PiecePtr piece() const noexcept { return piece_; }
    Position from() const noexcept { return from_; }
    Position to() const noexcept { return to_; }
    int startTime() const noexcept { return startTimeMs_; }
    int arrivalTime() const noexcept { return arrivalTimeMs_; }

private:
    PiecePtr piece_;
    Position from_;
    Position to_;
    int startTimeMs_;
    int arrivalTimeMs_;
};

}  // namespace kungfu