// Role: concrete implementation of the threat analyzer. It provides two key capabilities:
// isThreatened: quick check whether a given target square is currently threatened by at least one enemy piece.
// getThreateningPieces: return a detailed list of the specific enemy pieces threatening a given square.
// How it works: it reuses MoveGenerator
// to generate all of the opponent's potential moves and check whether one of them ends on the target square.
#pragma once

#include "engine/analysis/IThreatAnalyzer.hpp"
#include "engine/analysis/MoveGenerator.hpp"

namespace kungfu {

class ThreatAnalyzer : public IThreatAnalyzer {
public:
    ThreatAnalyzer() = default;
    ~ThreatAnalyzer() override = default;

    std::vector<view::PieceSnapshot> getThreateningPieces(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const override;

    bool isThreatened(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const override;
};

} // namespace kungfu