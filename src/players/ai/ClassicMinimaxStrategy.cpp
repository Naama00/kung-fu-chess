#include "players/ai/ClassicMinimaxStrategy.hpp"
#include "engine/common/PieceValues.hpp"
#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/PlayerAction.hpp"
#include <algorithm>
#include <limits>

namespace kungfu {

// פונקציית עזר פנימית ומהירה להערכת שווי חומרי בלבד ללא הקצאות זיכרון כלל!
int evaluateMaterialOnly(const view::GameSnapshot& snapshot, PlayerColor aiColor) {
    int score = 0;

    for (const auto& piece : snapshot.pieces) {
        if (piece.state == PieceState::Captured) continue;
        
        // שימוש במחלקה המרכזית החדשה במקום ב-lambda המקומית שהייתה משוכפלת
        int val = PieceValues::getCentipawnValue(piece.type);
        if (piece.color == aiColor) {
            score += val;
        } else {
            score -= val;
        }
    }
    return score;
}

ClassicMinimaxStrategy::ClassicMinimaxStrategy(int searchDepth, std::mt19937::result_type seed)
    : searchDepth_(searchDepth), rng_(seed) {}


ClassicMinimaxStrategy::PieceUndo ClassicMinimaxStrategy::applyMove(
    view::GameSnapshot& snapshot, const Position& from, const Position& to, PlayerColor color) const {
    
    PieceUndo undo{static_cast<std::size_t>(-1), from, PieceState::Idle, false, static_cast<std::size_t>(-1), snapshot.isGameOver};
    std::size_t movingPieceIndex = static_cast<std::size_t>(-1);

    for (std::size_t i = 0; i < snapshot.pieces.size(); ++i) {
        if (snapshot.pieces[i].logicalPosition == from && 
            snapshot.pieces[i].color == color && 
            snapshot.pieces[i].state != PieceState::Captured) {
            movingPieceIndex = i;
            break;
        }
    }

    if (movingPieceIndex == static_cast<std::size_t>(-1)) {
        return undo;
    }

    undo.pieceIndex = movingPieceIndex;
    undo.originalPos = snapshot.pieces[movingPieceIndex].logicalPosition;
    undo.originalState = snapshot.pieces[movingPieceIndex].state;
    undo.originalHasMoved = snapshot.pieces[movingPieceIndex].hasMoved;

    if (from != to) {
        for (std::size_t i = 0; i < snapshot.pieces.size(); ++i) {
            if (snapshot.pieces[i].logicalPosition == to && 
                snapshot.pieces[i].color != color && 
                snapshot.pieces[i].state != PieceState::Captured) {
                undo.capturedIndex = i;
                snapshot.pieces[i].state = PieceState::Captured;
                if (snapshot.pieces[i].type == PieceType::King) {
                    snapshot.isGameOver = true;
                }
                break;
            }
        }
    }

    snapshot.pieces[movingPieceIndex].logicalPosition = to;
    snapshot.pieces[movingPieceIndex].hasMoved = true;
    return undo;
}

void ClassicMinimaxStrategy::undoMove(view::GameSnapshot& snapshot, const PieceUndo& undo) const {
    if (undo.pieceIndex == static_cast<std::size_t>(-1)) return;

    snapshot.pieces[undo.pieceIndex].logicalPosition = undo.originalPos;
    snapshot.pieces[undo.pieceIndex].state = undo.originalState;
    snapshot.pieces[undo.pieceIndex].hasMoved = undo.originalHasMoved;

    if (undo.capturedIndex != static_cast<std::size_t>(-1)) {
        snapshot.pieces[undo.capturedIndex].state = PieceState::Idle;
    }
    snapshot.isGameOver = undo.originalIsGameOver;
}

int ClassicMinimaxStrategy::minimax(
    view::GameSnapshot& snapshot, int depth, int alpha, int beta, bool maximizingPlayer, PlayerColor aiColor) const {
    
    // בנקודת הקצה - אנו קוראים לפונקציית הערכת החומר המהירה שלנו
    if (depth == 0 || snapshot.isGameOver) {
        return evaluateMaterialOnly(snapshot, aiColor);
    }

    PlayerColor opponentColor = (aiColor == PlayerColor::White) ? PlayerColor::Black : PlayerColor::White;
    PlayerColor movingColor = maximizingPlayer ? aiColor : opponentColor;

    auto legalMoves = moveGenerator_.generateForPlayer(snapshot, movingColor);
    if (legalMoves.empty()) {
        return evaluateMaterialOnly(snapshot, aiColor);
    }

    if (maximizingPlayer) {
        int maxEval = std::numeric_limits<int>::min();
        for (const auto& move : legalMoves) {
            auto undo = applyMove(snapshot, move.from, move.to, movingColor);
            int eval = minimax(snapshot, depth - 1, alpha, beta, false, aiColor);
            undoMove(snapshot, undo);

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break; // קיצוץ אלפא-בטא
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (const auto& move : legalMoves) {
            auto undo = applyMove(snapshot, move.from, move.to, movingColor);
            int eval = minimax(snapshot, depth - 1, alpha, beta, true, aiColor);
            undoMove(snapshot, undo);

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break; // קיצוץ אלפא-בטא
        }
        return minEval;
    }
}

std::vector<ActionRequest> ClassicMinimaxStrategy::computeActions(const view::GameSnapshot& snapshot, PlayerColor aiColor) {
    const auto legalMoves = moveGenerator_.generateForPlayer(snapshot, aiColor);
    if (legalMoves.empty()) return {};

    int bestScore = std::numeric_limits<int>::min();
    std::vector<IMoveGenerator::MoveCandidate> bestMoves;
    view::GameSnapshot workingSnapshot = snapshot;

    for (const auto& move : legalMoves) {
        auto undo = applyMove(workingSnapshot, move.from, move.to, aiColor);
        int score = minimax(workingSnapshot, searchDepth_ - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, aiColor);
        undoMove(workingSnapshot, undo);

        if (score > bestScore) {
            bestScore = score;
            bestMoves.clear();
            bestMoves.push_back(move);
        } else if (score == bestScore) {
            bestMoves.push_back(move);
        }
    }

    if (bestMoves.empty()) return {};

    std::uniform_int_distribution<std::size_t> dist(0, bestMoves.size() - 1);
    const auto selected = bestMoves[dist(rng_)];

    return { ActionRequest(0, aiColor, PlayerAction(selected.from, selected.to)) };
};

} // namespace kungfu