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

        // Collect moves from all pieces that are not in cooldown
        for (const auto& piece : snapshot.pieces) {
            if (piece.color != aiColor || piece.state == PieceState::Captured) continue;
            if (piece.cooldownProgress > 0.0f || piece.state == PieceState::Moving || piece.state == PieceState::Airborne) continue;

            auto moves = moveGenerator_.generateForPiece(snapshot, piece.logicalPosition);
            availableMoves.insert(availableMoves.end(), moves.begin(), moves.end());
        }

        if (availableMoves.empty()) return {};

        // The easy level moves only one completely random piece without any tactical checks
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

            // Check whether the piece itself is threatened in its current position
            bool threatened = isSquareThreatened(snapshot, piece.logicalPosition, aiColor);

            IMoveGenerator::MoveCandidate bestMove = moves[0];
            int bestScore = -1;

            for (const auto& m : moves) {
                int score = evaluateImmediateCapture(snapshot, m.to);

                // If the piece is threatened, prefer escape moves to an unthreatened square
                if (threatened && !isSquareThreatened(snapshot, m.to, aiColor)) {
                    score += PieceValues::getCentipawnValue(piece.type) / 2;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestMove = m;
                }
            }

            // Add this piece's move
            selectedActions.emplace_back(0, aiColor, PlayerAction(bestMove.from, bestMove.to));

            // Simultaneous limit: Medium moves up to 2 pieces in parallel
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

                // Prevent moving a piece into a threatened square
                if (isSquareThreatened(snapshot, m.to, aiColor)) {
                    score -= PieceValues::getCentipawnValue(piece.type);
                }

                // Highest priority is to escape threats
                if (threatened && !isSquareThreatened(snapshot, m.to, aiColor)) {
                    score += PieceValues::getCentipawnValue(piece.type);
                }

                // Increased priority for attacking the opponent king
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
            // There is no simultaneous limit in Hard mode (it moves the entire army of pieces in perfect coordination!)
        }

        return selectedActions;
    }
};

} // namespace kungfu