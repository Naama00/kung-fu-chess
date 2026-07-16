// src/rules/PieceRules.cpp
#include "engine/rules/PieceRules.hpp"
#include "engine/common/GameConfig.hpp"
#include <algorithm>
#include <unordered_map>

namespace kungfu {

namespace {

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
            if (targetPieceOpt.value()->color() != piece.color()) {
                dests.push_back(target); 
            }
            break; 
        }
        r += rowStep;
        c += colStep;
    }
}

} // namespace

// --- Rook ---
std::vector<Position> RookRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(14);
    getSlidingDestinations(board, piece, 1, 0, dests);  
    getSlidingDestinations(board, piece, -1, 0, dests); 
    getSlidingDestinations(board, piece, 0, 1, dests);  
    getSlidingDestinations(board, piece, 0, -1, dests); 
    return dests;
}

// --- Bishop ---
std::vector<Position> BishopRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(13);
    getSlidingDestinations(board, piece, 1, 1, dests);   
    getSlidingDestinations(board, piece, 1, -1, dests);  
    getSlidingDestinations(board, piece, -1, 1, dests);  
    getSlidingDestinations(board, piece, -1, -1, dests); 
    return dests;
}

// --- Queen ---
std::vector<Position> QueenRule::getLegalDestinations(const IBoard& board, const Piece& piece) const {
    std::vector<Position> dests;
    dests.reserve(27);

    const auto& rook = PieceRuleFactory::getRule(PieceType::Rook);
    const auto& bishop = PieceRuleFactory::getRule(PieceType::Bishop);

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
            dests.push_back(Position(r, c));
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
    dests.reserve(4);

    int currentRow = piece.position().row();
    int currentCol = piece.position().col();
    int maxRows = board.rows();
    int maxCols = board.cols();

    int direction = (piece.color() == PlayerColor::White) ? -1 : 1;

    // עוזר: האם יש כלי סטטי (לא Moving/Airborne) בעמדה נתונה
    auto hasStaticPiece = [&](const Position& pos) -> bool {
        auto opt = board.pieceAt(pos);
        if (!opt.has_value() || !opt.value()) return false;
        auto state = opt.value()->state();
        return state != PieceState::Moving && state != PieceState::Airborne;
    };

    // עוזר: האם יש כלי אויב סטטי בעמדה נתונה
    auto hasStaticEnemy = [&](const Position& pos) -> bool {
        auto opt = board.pieceAt(pos);
        if (!opt.has_value() || !opt.value()) return false;
        auto state = opt.value()->state();
        if (state == PieceState::Moving || state == PieceState::Airborne) return false;
        return opt.value()->color() != piece.color();
    };

    int nextRow = currentRow + direction;
    if (nextRow >= 0 && nextRow < maxRows) {
        Position forward(nextRow, currentCol);
        // מהלך קדימה: רק אם אין כלי סטטי (כלי בתנועה לא חוסם)
        bool isForwardEmpty = !hasStaticPiece(forward);

        if (isForwardEmpty) {
            dests.push_back(forward);
        }

        if (!piece.hasMoved()) {
            int doubleRow = currentRow + 2 * direction;
            if (doubleRow >= 0 && doubleRow < maxRows) {
                Position doubleForward(doubleRow, currentCol);
                if (isForwardEmpty && !hasStaticPiece(doubleForward)) {
                    dests.push_back(doubleForward);
                }
            }
        }

        // לכידה אלכסונית: רק אם יש אויב סטטי שם (לא כלי בתנועה)
        for (int cOffset : {-1, 1}) {
            int nextCol = currentCol + cOffset;
            if (nextCol >= 0 && nextCol < maxCols) {
                Position diagonal(nextRow, nextCol);
                if (hasStaticEnemy(diagonal)) {
                    dests.push_back(diagonal);
                }
            }
        }
    }

    return dests;
}

// --- PieceRuleFactory Dynamic Registry Implementation ---
static std::unordered_map<PieceType, std::unique_ptr<IPieceRule>>& getRegistry() noexcept {
    static std::unordered_map<PieceType, std::unique_ptr<IPieceRule>> registry;
    if (registry.empty()) {
        registry[PieceType::Rook] = std::make_unique<RookRule>();
        registry[PieceType::Bishop] = std::make_unique<BishopRule>();
        registry[PieceType::Queen] = std::make_unique<QueenRule>();
        registry[PieceType::Knight] = std::make_unique<KnightRule>();
        registry[PieceType::King] = std::make_unique<KingRule>();
        registry[PieceType::Pawn] = std::make_unique<PawnRule>();
    }
    return registry;
}

const IPieceRule& PieceRuleFactory::getRule(PieceType type) noexcept {
    auto& reg = getRegistry();
    auto it = reg.find(type);
    if (it != reg.end() && it->second) {
        return *(it->second);
    }
    static const PawnRule defaultRule;
    return defaultRule;
}

void PieceRuleFactory::registerRule(PieceType type, std::unique_ptr<IPieceRule> rule) noexcept {
    getRegistry()[type] = std::move(rule);
}

}  // namespace kungfu