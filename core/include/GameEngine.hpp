#ifndef GAME_ENGINE_HPP
#define GAME_ENGINE_HPP

#include "Board.hpp"
#include "Position.hpp"
#include <vector>

class GameEngine {
private:
    Board board;
    long long currentTimeMs;
    Position selectedPos;
    bool hasSelection;
    std::vector<PendingMove> activeMoves; // ניהול התנועות בזמן אמת

    // פונקציה פנימית שמחשבת כמה זמן לוקח לכלי לזוז
    long long calculateDurationMs(const std::string& token, Position src, Position dest);

public:
    GameEngine();
    
    void initBoard(const Board& initialBoard);
    void handleClick(int x, int y);
    void advanceTime(long long ms);
    
    // החזרת מצב הלוח העדכני לזמן הנוכחי עבור פקודת ההדפסה
    Board getRenderedBoard() const;
};

#endif