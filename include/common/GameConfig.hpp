#pragma once

namespace kungfu {

struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

    static constexpr int kDefaultCellSize = 100;
    static constexpr int kCooldownDurationMs = 2000;
    static constexpr int kMsPerCellSpeed = 1000;
    
    // קבוע לשמירת משך זמן הקפיצה במקום
    static constexpr int kJumpDurationMs = 1000;

    // הגדרות חדשות לתכונות המתקדמות בזמן אמת:
    static constexpr bool kAllowSimultaneousMovement = true; // הדלקת תנועה סימולטנית במקביל
    static constexpr bool kEnablePremoves = true;            // הדלקת תור מהלכים מראש
};

}  // namespace kungfu