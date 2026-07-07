#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <sstream>
#include <string_view>
#include <cmath>
#include <algorithm>


struct Position {
    int row;
    int col;

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
};

struct Point2D {
    double x;
    double y;
};

struct PendingMove {
    std::string token;
    Position src;
    Position dest;
    long long arrivalTimeMs;
    bool isActive = false;
};



#include <string>

enum class Color { White, Black, None };

class Piece {
private:
    std::string type; // למשל "wK", "bP", "."
    Color color;
    Position pos;
    bool moving;

public:
    Piece() : type("."), color(Color::None), pos({0, 0}), moving(false) {}
    Piece(std::string t, Position p) : type(t), pos(p), moving(false) {
        if (t == ".") color = Color::None;
        else color = (t[0] == 'w') ? Color::White : Color::Black;
    }

    std::string getType() const { return type; }
    Color getColor() const { return color; }
    Position getPosition() const { return pos; }
    void setPosition(Position p) { pos = p; }
};




#include <vector>
#include <string>
#include <string_view>
#include <unordered_set>

class Board {
private:
    std::vector<std::vector<std::string>> grid;

public:
    Board() = default;
    
    void clear() { grid.clear(); }
    void addRow(const std::vector<std::string>& row) { grid.push_back(row); }
    
    const std::vector<std::vector<std::string>>& getGrid() const { return grid; }
    bool isEmpty() const { return grid.empty(); }
    size_t getWidth() const { return grid.empty() ? 0 : grid[0].size(); }
    size_t getHeight() const { return grid.size(); }
    
    std::string getTokenAt(int r, int c) const { return grid[r][c]; }
    void setTokenAt(int r, int c, const std::string& token) { grid[r][c] = token; }

    // העברת המימוש לכאן כפונקציית inline מונעת שגיאות לינקר במערכות אוטומטיות
    static bool isValidToken(std::string_view token) {
        static const std::unordered_set<std::string_view> validTokens = {
            ".", "wK", "wQ", "wR", "wB", "wN", "wP",
            "bK", "bQ", "bR", "bB", "bN", "bP"
        };
        return validTokens.find(token) != validTokens.end();
    }

    static bool isLegalMovePattern(const std::string& pieceType, int srcRow, int srcCol, int destRow, int destCol);
};




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




#include <string>

class PathValidator {
public:
    // בודק שהמסלול בין המקור ליעד אינו חסום (עבור רץ וצריח)
    static bool isPathClear(const Board& board, int srcRow, int srcCol, int destRow, int destCol);
    
    // בודק את חוקיות נקודת היעד (אי אפשר לאכול כלי מאותו צבע)
    static bool isValidTarget(const Board& board, int srcRow, int srcCol, int destRow, int destCol);
};




#include <ostream>

class BoardFormatter {
public:
    static void print(std::ostream& output, const Board& board);
};



#include <istream>

enum class ParseResult {
    Success,
    UnknownToken,
    RowWidthMismatch
};

class BoardParser {
public:
    // הפונקציה מקבלת stream כלשהו (זה יכול להיות cin או קובץ בטסטים שלך)
    static ParseResult parse(std::istream& input, Board& outBoard);
};



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


void BoardFormatter::print(std::ostream& output, const Board& board) {
    const auto& grid = board.getGrid();
    for (const auto& row : grid) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) {
                output << ' ';
            }
            output << row[i];
        }
        output << '\n';
    }
}

#include <sstream>

ParseResult BoardParser::parse(std::istream& input, Board& outBoard) {
    outBoard.clear();
    std::string line;

    // קריאת שורת הכותרת "Board:"
    if (!std::getline(input, line)) {
        return ParseResult::Success;
    }

    while (std::getline(input, line)) {
        if (line == "Commands:") {
            break;
        }
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::vector<std::string> row;
        std::string token;

        while (ss >> token) {
            if (!Board::isValidToken(token)) {
                return ParseResult::UnknownToken;
            }
            row.push_back(token);
        }

        if (!outBoard.isEmpty() && row.size() != outBoard.getWidth()) {
            return ParseResult::RowWidthMismatch;
        }

        outBoard.addRow(row);
    }

    return ParseResult::Success;
}

#include <iostream>
#include <string>
#include <sstream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Board board;
    ParseResult result = BoardParser::parse(std::cin, board);

    if (result == ParseResult::UnknownToken) {
        std::cout << "ERROR UNKNOWN_TOKEN\n";
        return 0;
    } else if (result == ParseResult::RowWidthMismatch) {
        std::cout << "ERROR ROW_WIDTH_MISMATCH\n";
        return 0;
    }

    GameEngine engine;
    engine.initBoard(board);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "click") {
            int x, y;
            if (ss >> x >> y) {
                engine.handleClick(x, y);
            }
        } 
        else if (command == "wait") {
            long long ms;
            if (ss >> ms) {
                engine.advanceTime(ms);
            }
        } 
        else if (command == "print" && ss >> command && command == "board") {
            BoardFormatter::print(std::cout, engine.getRenderedBoard());
        }   
    }

    return 0;
}

