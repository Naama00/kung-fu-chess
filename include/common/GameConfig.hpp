#pragma once

namespace kungfu {

struct GameConfig {
    static constexpr int kBoardSize = 8;
    static constexpr int kDefaultMoveStep = 1;

    // Pawn start rows (the row a pawn sits on at game start).
    static constexpr int kWhitePawnStartRow = 1;
    static constexpr int kBlackPawnStartRow = 6;

};

}  // namespace kungfu
