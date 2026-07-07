#include "Board.hpp"
#include <cmath>
#include <cstdlib>

bool Board::isLegalMovePattern(const std::string& pieceType, int srcRow, int srcCol, int destRow, int destCol) {
    if (srcRow == destRow && srcCol == destCol) return false;

    char color = pieceType[0]; // 'w' או 'b'
    char type = pieceType[1];  // K, Q, R, B, N, P

    int dRow = std::abs(destRow - srcRow);
    int dCol = std::abs(destCol - srcCol);

    // חוקי הרגלי (Pawn)
    if (type == 'P') {
        int rowDirection = (color == 'w') ? -1 : 1; // לבן עולה (שורות קטנות), שחור יורד (שורות גדלות)
        
        // א. תנועה צעד אחד קדימה (בטור ישר בלבד)
        if (destCol == srcCol && (destRow - srcRow) == rowDirection) {
            return true;
        }
        
        // ב. תנועה צעד אחד באלכסון (עבור הכאה - אימות סופי יבוצע ב-PathValidator)
        if (dCol == 1 && (destRow - srcRow) == rowDirection) {
            return true;
        }
        
        return false;
    }

    // שאר הכלים (נשארים ללא שינוי)
    switch (type) {
        case 'K': return (dRow <= 1 && dCol <= 1);
        case 'R': return (srcRow == destRow || srcCol == destCol);
        case 'B': return (dRow == dCol);
        case 'Q': return (srcRow == destRow || srcCol == destCol || dRow == dCol);
        case 'N': return ((dRow == 2 && dCol == 1) || (dRow == 1 && dCol == 2));
        default: return false;
    }
}