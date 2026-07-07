#include "PathValidator.hpp"
#include <cmath>
#include <algorithm>

// בתוך core/src/PathValidator.cpp

bool PathValidator::isValidTarget(const Board& board, int srcRow, int srcCol, int destRow, int destCol) {
    std::string srcToken = board.getTokenAt(srcRow, srcCol);
    std::string destToken = board.getTokenAt(destRow, destCol);

    if (destToken == ".") {
        // רגלי נע ישר רק למשבצת ריקה. אם הוא מנסה לנוע באלכסון למשבצת ריקה - זה לא חוקי
        if (srcToken[1] == 'P' && srcCol != destCol) {
            return false;
        }
        return true; 
    }
    
    // אם יש שם כלי - אסור לאכול כלי מאותו צבע
    if (srcToken[0] == destToken[0]) return false;

    // רגלי יכול להכות באלכסון בלבד. אם הוא מנסה לנוע ישר למשבצת עם כלי - התנועה חסומה
    if (srcToken[1] == 'P' && srcCol == destCol) {
        return false;
    }

    return true;
}

bool PathValidator::isPathClear(const Board& board, int srcRow, int srcCol, int destRow, int destCol) {
    std::string pieceToken = board.getTokenAt(srcRow, srcCol);
    char type = pieceToken[1];

    // פרש (N) יכול לקפוץ מעל כלים, לכן המסלול שלו תמיד נחשב פנוי
    if (type == 'N') return true;

    int rowStep = (destRow - srcRow == 0) ? 0 : ((destRow - srcRow) > 0 ? 1 : -1);
    int colStep = (destCol - srcCol == 0) ? 0 : ((destCol - srcCol) > 0 ? 1 : -1);

    int currRow = srcRow + rowStep;
    int currCol = srcCol + colStep;

    // סריקת כל המשבצות בדרך (לא כולל משבצת היעד עצמה)
    while (currRow != destRow || currCol != destCol) {
        if (board.getTokenAt(currRow, currCol) != ".") {
            return false; // נמצאה חסימה בדרך
        }
        currRow += rowStep;
        currCol += colStep;
    }

    return true;
}