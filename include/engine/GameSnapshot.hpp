#pragma once

#include <vector>
#include <optional>
#include "common/Position.hpp"
#include "common/Enums.hpp"

namespace kungfu {

struct PieceSnapshot {
    PieceType type;
    PlayerColor color;
    Position logicalPosition;
    PieceState state;
    
    // קואורדינטות פיקסל מדויקות לציור (מחושבות מראש כולל אינטרפולציה בזמן תנועה)
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

}  // namespace kungfu