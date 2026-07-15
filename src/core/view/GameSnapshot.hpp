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

    // אחוז התקדמות הצינון (0.0f = מוכן, >0.0f = בטעינה). מחושב מראש כאן,
    // בזמן בניית ה-Snapshot, כדי שכל צרכן (View) לא יצטרך לבצע חיפוש נוסף
    // אחר הכלי בלוח כדי לשאול את ה-Arbiter בעצמו.
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