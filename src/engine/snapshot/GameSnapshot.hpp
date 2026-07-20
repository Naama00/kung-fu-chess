#pragma once

#include <vector>
#include <optional>
#include "engine/common/Position.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {
namespace view {

struct PieceSnapshot {
    PieceType type;
    PlayerColor color;
    Position logicalPosition;
    PieceState state;
    bool hasMoved = false;
    
    // Drawing positions in pixels for the Renderer
    float pixelX;
    float pixelY;

    // Cooldown progress percentage (0.0f = ready, >0.0f = charging). Precomputed here,
    // During snapshot construction, so every consumer (View) does not need to perform an extra search
    // after the piece on the board to query the Arbiter itself.
    float cooldownProgress = 0.0f;
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