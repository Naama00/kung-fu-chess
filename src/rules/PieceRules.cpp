#include "rules/PieceRules.hpp"
#include "common/GameConfig.hpp"
#include <algorithm>

namespace kungfu {

namespace {

// פונקציית עזר פנימית לחישוב תנועת החלקה (Sliding) עבור צריח, רץ ומלכה
void getSlidingDestinations(const IBoard& board, const Piece& piece, int rowStep, int colStep, std::vector<Position>& dests) {
    int r = piece.position().row() + rowStep;
    int c = piece.position().col() + colStep;
    int maxRows = board.rows();
    int maxCols = board.cols();

    while (r >= 0 && r < maxRows && c >= 0 && c < maxCols) {
        Position target(r, c);
        auto targetPieceOpt = board.pieceAt(target);

        if (!targetPieceOpt.has_value()) {
            dests.push_back(target);
        } else {
            // אם יש כלי ביעד, הוא חוסם את המשך ההחלקה
            if (targetPieceOpt.value()->color() != piece.color()) {
                dests.push_back(target); // אכילה אפשרית ביעד
            }
            break; // חסימה - יוצאים מהלולאה
        }
        r += rowStep;
        c += colStep;
    }
}

} // namespace

// --- Rook ---
std::vector<Position> RookRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(14); // הערכה סבירה למקסימום משבצות חופשיות לצריח
    getSlidingDestinations(board, piece, 1, 0, dests);  // למטה
    getSlidingDestinations(board, piece, -1, 0, dests); // למעלה
    getSlidingDestinations(board, piece, 0, 1, dests);  // ימינה
    getSlidingDestinations(board, piece, 0, -1, dests); // שמאלה
    return dests;
}

// --- Bishop ---
std::vector<Position> BishopRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(13);
    getSlidingDestinations(board, piece, 1, 1, dests);   // אלכסון ימין-מטה
    getSlidingDestinations(board, piece, 1, -1, dests);  // אלכסון שמאל-מטה
    getSlidingDestinations(board, piece, -1, 1, dests);  // אלכסון ימין-מעלה
    getSlidingDestinations(board, piece, -1, -1, dests); // אלכסון שמאל-מעלה
    return dests;
}

// --- Queen ---
std::vector<Position> QueenRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(27);
    
    // שילוב יעיל של לוגיקת הצריח והרץ למניעת כפל קוד
    RookRule rook;
    BishopRule bishop;
    
    auto rookDests = rook.getLegalDestinations(board, piece);
    auto bishopDests = bishop.getLegalDestinations(board, piece);
    
    dests.insert(dests.end(), rookDests.begin(), rookDests.end());
    dests.insert(dests.end(), bishopDests.begin(), bishopDests.end());
    
    return dests;
}

// --- Knight ---
std::vector<Position> KnightRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(8);

    static const std::vector<std::pair<int, int>> knightOffsets = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };

    int currentRow = piece.position().row();
    int currentCol = piece.position().col();
    int maxRows = board.rows();
    int maxCols = board.cols();

    for (const auto& offset : knightOffsets) {
        int r = currentRow + offset.first;
        int c = currentCol + offset.second;

        if (r >= 0 && r < maxRows && c >= 0 && c < maxCols) {
            Position target(r, c);
            auto targetPieceOpt = board.pieceAt(target);
            if (!targetPieceOpt.has_value() || targetPieceOpt.value()->color() != piece.color()) {
                dests.push_back(target);
            }
        }
    }
    return dests;
}

// --- King ---
std::vector<Position> KingRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(8);

    int currentRow = piece.position().row();
    int currentCol = piece.position().col();
    int maxRows = board.rows();
    int maxCols = board.cols();

    for (int rOffset = -1; rOffset <= 1; ++rOffset) {
        for (int cOffset = -1; cOffset <= 1; ++cOffset) {
            if (rOffset == 0 && cOffset == 0) continue;

            int r = currentRow + rOffset;
            int c = currentCol + cOffset;

            if (r >= 0 && r < maxRows && c >= 0 && c < maxCols) {
                Position target(r, c);
                auto targetPieceOpt = board.pieceAt(target);
                if (!targetPieceOpt.has_value() || targetPieceOpt.value()->color() != piece.color()) {
                    dests.push_back(target);
                }
            }
        }
    }
    return dests;
}

// --- Pawn (Simplified) ---
std::vector<Position> PawnRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(4); // הקצאה פוטנציאלית ל-4 יעדים (צעד, צעד כפול, 2 אלכסונים)

    int currentRow = piece.position().row();
    int currentCol = piece.position().col();
    int maxRows = board.rows();
    int maxCols = board.cols();

    // קביעת כיוון התקדמות (לבן עולה, שחור יורד)
    int direction = (piece.color() == PlayerColor::White) ? 1 : -1;

    // 1. צעד אחד קדימה (רק אם המשבצת ריקה)
    int nextRow = currentRow + direction;
    if (nextRow >= 0 && nextRow < maxRows) {
        Position forward(nextRow, currentCol);
        bool isForwardEmpty = !board.pieceAt(forward).has_value();
        
        if (isForwardEmpty) {
            dests.push_back(forward);
        }

        // 2. צעד כפול קדימה משורת ההתחלה (רק אם שני התאים קדימה ריקים)
        int startRow = (piece.color() == PlayerColor::White) ? GameConfig::kWhitePawnStartRow : GameConfig::kBlackPawnStartRow;
        if (currentRow == startRow) {
            int doubleRow = currentRow + 2 * direction;
            if (doubleRow >= 0 && doubleRow < maxRows) {
                Position doubleForward(doubleRow, currentCol);
                // בדיקת "נתיב פנוי" - גם משבצת הביניים וגם היעד חייבות להיות ריקות
                if (isForwardEmpty && !board.pieceAt(doubleForward).has_value()) {
                    dests.push_back(doubleForward);
                }
            }
        }

        // 3. אכילה באלכסון (רק אם יש שם כלי עוין)
        for (int cOffset : {-1, 1}) {
            int nextCol = currentCol + cOffset;
            if (nextCol >= 0 && nextCol < maxCols) {
                Position diagonal(nextRow, nextCol);
                auto targetPieceOpt = board.pieceAt(diagonal);
                if (targetPieceOpt.has_value() && targetPieceOpt.value()->color() != piece.color()) {
                    dests.push_back(diagonal);
                }
            }
        }
    }

    return dests;
}

// --- Factory ---
const IPieceRule& PieceRuleFactory::getRule(PieceType type) noexcept {
    static const RookRule rookRule;
    static const BishopRule bishopRule;
    static const QueenRule queenRule;
    static const KnightRule knightRule;
    static const KingRule kingRule;
    static const PawnRule pawnRule;

    switch (type) {
        case PieceType::Rook:   return rookRule;
        case PieceType::Bishop: return bishopRule;
        case PieceType::Queen:  return queenRule;
        case PieceType::Knight: return knightRule;
        case PieceType::King:   return kingRule;
        case PieceType::Pawn:   return pawnRule;
    }
    return pawnRule; // ברירת מחדל בטוחה
}

}  // namespace kungfu