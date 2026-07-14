#pragma once

namespace kungfu {

// קונפיגורציית ריצה של מנוע המשחק - כל הערכים ניתנים לשינוי
struct GameConfig {
    int cooldownDurationMs = 2000;
    int msPerCellSpeed = 1000;
    int jumpDurationMs = 1000;

    bool allowSimultaneousMovement = true;
    bool enablePremoves = true;
    bool allowJumping = true; 
};

}  // namespace kungfu