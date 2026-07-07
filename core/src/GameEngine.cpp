#include "GameEngine.hpp"
#include "PathValidator.hpp"
#include <iostream>

GameEngine::GameEngine() : currentTimeMs(0), selectedPos({-1, -1}), hasSelection(false) {}

void GameEngine::initBoard(const Board& initialBoard) {
    board = initialBoard;
}

// פונקציית הזמן המרכזית - כרגע שמתי ערך ברירת מחדל של 1000 מילישניות (שנייה אחת).
// ברגע שתדעי את הנוסחה המדויקת מהטסטים, נשנה רק את השורה הזו!
long long GameEngine::calculateDurationMs(const std::string& token, Position src, Position dest) {
    return 1000; 
}

void GameEngine::handleClick(int x, int y) {
    int col = x / 100;
    int row = y / 100;

    if (row < 0 || row >= static_cast<int>(board.getHeight()) || 
        col < 0 || col >= static_cast<int>(board.getWidth())) {
        return;
    }

    // לקבלת המצב העדכני ביותר (כדי למנוע לחיצה על כלי שכבר זז)
    Board currentBoard = getRenderedBoard();
    std::string token = currentBoard.getTokenAt(row, col);

    if (!hasSelection) {
        if (token != ".") {
            selectedPos = {row, col};
            hasSelection = true;
        }
    } else {
        std::string srcToken = currentBoard.getTokenAt(selectedPos.row, selectedPos.col);
        char srcColor = srcToken[0];
        char destColor = token[0];

        if (token != "." && srcColor == destColor) {
            selectedPos = {row, col};
        } else {
            // אימות המהלך מול הלוח הנוכחי
            if (Board::isLegalMovePattern(srcToken, selectedPos.row, selectedPos.col, row, col) &&
                PathValidator::isValidTarget(currentBoard, selectedPos.row, selectedPos.col, row, col) &&
                PathValidator::isPathClear(currentBoard, selectedPos.row, selectedPos.col, row, col)) {
                
                // במקום להזיז מיד, אנחנו רושמים תנועה עתידית ומפנים את משבצת המקור בלוח הראשי
                long long duration = calculateDurationMs(srcToken, selectedPos, {row, col});
                PendingMove move{srcToken, selectedPos, {row, col}, currentTimeMs + duration, true};
                activeMoves.push_back(move);
                
                // אופציונלי: מוחקים מהלוח המקורי את המקור כדי שלא יחשב כשני כלים, 
                // היעד יתעדכן בלוח הראשי רק כשהזמן יגיע.
                board.setTokenAt(selectedPos.row, selectedPos.col, ".");
            }
            hasSelection = false;
        }
    }
}

void GameEngine::advanceTime(long long ms) {
    currentTimeMs += ms;
    
    // בדיקה האם תנועות הסתיימו ועדכון הלוח המרכזי לצמיתות
    for (auto& move : activeMoves) {
        if (move.isActive && currentTimeMs >= move.arrivalTimeMs) {
            board.setTokenAt(move.dest.row, move.dest.col, move.token);
            move.isActive = false; // התנועה הושלמה
        }
    }
}

Board GameEngine::getRenderedBoard() const {
    // מייצרים עותק של הלוח הבסיסי ומציגים עליו את הכלים לפי מצב הזמן הנוכחי
    Board rendered = board;
    
    for (const auto& move : activeMoves) {
        if (move.isActive) {
            if (currentTimeMs < move.arrivalTimeMs) {
                // הזמן עוד לא הגיע - הכלי עדיין מוצג במיקום המקור שלו!
                rendered.setTokenAt(move.src.row, move.src.col, move.token);
            } else {
                // הזמן עבר - הכלי מוצג ביעד
                rendered.setTokenAt(move.dest.row, move.dest.col, move.token);
            }
        }
    }
    return rendered;
}