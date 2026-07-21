// engine/snapshot/GameSnapshot.hpp
#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include "engine/common/Position.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {
namespace view {

struct PieceSnapshot {
    std::uint64_t id;
    PieceType type;
    PlayerColor color;
    Position logicalPosition;
    PieceState state;
    bool hasMoved = false;
    float cooldownProgress = 0.0f;

    // Temporal motion properties to describe transitions logically (independent of pixel resolution)
    bool isMoving = false;
    Position transitFrom;
    Position transitTo;
    int startTimeMs = 0;
    int arrivalTimeMs = 0;
};

struct GameSnapshot {
    int boardCols;
    int boardRows;
    std::vector<PieceSnapshot> pieces;
    std::optional<Position> selectedCell;
    bool isGameOver;
};

}  // namespace view
}  // namespace kungfu