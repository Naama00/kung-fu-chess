// Role: abstract interface defining functions for identifying threats to specific board cells.
#pragma once

#include <vector>
#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/common/Position.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

class IThreatAnalyzer {
public:
    virtual ~IThreatAnalyzer() = default;

    // Checks which opponent pieces threaten the target position.
    virtual std::vector<view::PieceSnapshot> getThreateningPieces(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const = 0;

    // Fast check if target position is threatened by any opponent.
    virtual bool isThreatened(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const = 0;
};

} // namespace kungfu