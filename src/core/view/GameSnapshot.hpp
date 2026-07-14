#pragma once

#include <vector>
#include <optional>
#include "core/common/Position.hpp"
#include "core/common/Enums.hpp"

namespace kungfu {
namespace view {

struct PieceSnapshot {
    PieceType type;
    PlayerColor color;
    Position logicalPosition;
    PieceState state;
    
    // מיקומי ציור בפיקסלים לטובת ה-Renderer
    float pixelX;
    float pixelY;
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