// players/ai/RealTimeStrategies.hpp
#pragma once

#include "players/ai/RealTimeBaseStrategy.hpp"
#include <random>

namespace kungfu {

// =================== 1. EASY STRATEGY ===================
class RealTimeEasyStrategy : public RealTimeBaseStrategy {
private:
    mutable std::mt19937 rng_{std::random_device{}()};

public:
    std::vector<ActionRequest> computeActions(const view::GameSnapshot& snapshot, PlayerColor aiColor) override {
        std::vector<IMoveGenerator::MoveCandidate> availableMoves;

        // איסוף מהלכים מכל הכלים שאינם בצינון
        for (const auto& piece : snapshot.pieces) {
            if (piece.color != aiColor || piece.state == PieceState::Captured) continue;
            if (piece.cooldownProgress > 0.0f || piece.state == PieceState::Moving || piece.state == PieceState::Airborne) continue;

            auto moves = moveGenerator_.generateForPiece(snapshot, piece.logicalPosition);
            availableMoves.insert(availableMoves.end(), moves.begin(), moves.end());
        }

        if (availableMoves.empty()) return {};

        // הרמה הקלה מזיזה רק כלי אחד אקראי לחלוטין ללא שום בדיקה טקטית
        std::uniform_int_distribution<std::size_t> dist(0, availableMoves.size() - 1);
        auto selected = availableMoves[dist(rng_)];

        return { ActionRequest(0, aiColor, PlayerAction(selected.from, selected.to)) };
    }
};

// =================== 2. MEDIUM STRATEGY ===================
class RealTimeMediumStrategy : public RealTimeBaseStrategy {
private:
    mutable std::mt19937 rng_{std::random_device{}()};

public:
    std::vector<ActionRequest> computeActions(const view::GameSnapshot& snapshot, PlayerColor aiColor) override {
        std::vector<ActionRequest> selectedActions;

        for (const auto& piece : snapshot.pieces) {
            if (piece.color != aiColor || piece.state == PieceState::Captured) continue;
            if (piece.cooldownProgress > 0.0f || piece.state == PieceState::Moving || piece.state == PieceState::Airborne) continue;

            auto moves = moveGenerator_.generateForPiece(snapshot, piece.logicalPosition);
            if (moves.empty()) continue;

            // בדיקה האם הכלי עצמו מאוים במיקומו הנוכחי
            bool threatened = isSquareThreatened(snapshot, piece.logicalPosition, aiColor);

            IMoveGenerator::MoveCandidate bestMove = moves[0];
            int bestScore = -1;

            for (const auto& m : moves) {
                int score = evaluateImmediateCapture(snapshot, m.to);

                // אם הכלי מאוים, עדיפות למהלכי בריחה למשבצת שאינה מאוימת
                if (threatened && !isSquareThreatened(snapshot, m.to, aiColor)) {
                    score += getPieceValue(piece.type) / 2;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestMove = m;
                }
            }

            // הוספת המהלך של כלי זה
            selectedActions.emplace_back(0, aiColor, PlayerAction(bestMove.from, bestMove.to));

            // מגבלה סימולטנית: רמת Medium מזיזה עד 2 כלים במקביל
            if (selectedActions.size() >= 2) {
                break;
            }
        }

        return selectedActions;
    }
};

// =================== 3. HARD STRATEGY ===================
class RealTimeHardStrategy : public RealTimeBaseStrategy {
public:
    std::vector<ActionRequest> computeActions(const view::GameSnapshot& snapshot, PlayerColor aiColor) override {
        std::vector<ActionRequest> selectedActions;

        for (const auto& piece : snapshot.pieces) {
            if (piece.color != aiColor || piece.state == PieceState::Captured) continue;
            if (piece.cooldownProgress > 0.0f || piece.state == PieceState::Moving || piece.state == PieceState::Airborne) continue;

            auto moves = moveGenerator_.generateForPiece(snapshot, piece.logicalPosition);
            if (moves.empty()) continue;

            bool threatened = isSquareThreatened(snapshot, piece.logicalPosition, aiColor);

            IMoveGenerator::MoveCandidate bestMove = moves[0];
            int bestScore = -1000;

            for (const auto& m : moves) {
                int score = evaluateImmediateCapture(snapshot, m.to);

                // מניעת כניסה של כלי למשבצת מאוימת
                if (isSquareThreatened(snapshot, m.to, aiColor)) {
                    score -= getPieceValue(piece.type);
                }

                // עדיפות עליונה לבריחה מאיומים
                if (threatened && !isSquareThreatened(snapshot, m.to, aiColor)) {
                    score += getPieceValue(piece.type);
                }

                // עדיפות מוגברת לתקיפת המלך היריב
                for (const auto& target : snapshot.pieces) {
                    if (target.logicalPosition == m.to && target.type == PieceType::King) {
                        score += 50000;
                    }
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestMove = m;
                }
            }

            selectedActions.emplace_back(0, aiColor, PlayerAction(bestMove.from, bestMove.to));
            // אין מגבלת סימולטניות ל-Hard (מזיז את כל צבא הכלים בתיאום מושלם!)
        }

        return selectedActions;
    }
};

} // namespace kungfu