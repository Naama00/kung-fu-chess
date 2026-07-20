// Role: abstract interface defining the contract for generating moves. 
// Declares methods for generating moves for a single piece or for an entire player based on a game snapshot.
#pragma once

#include <vector>
#include "engine/actions/ActionRequest.hpp"
#include "engine/common/Position.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace kungfu {

class IMoveGenerator {
public:
    struct MoveCandidate {
        Position from;
        Position to;
    };

    virtual ~IMoveGenerator() = default;

    virtual std::vector<MoveCandidate> generateForPiece(
        const view::GameSnapshot& snapshot,
        const Position& piecePosition) const = 0;

    virtual std::vector<MoveCandidate> generateForPlayer(
        const view::GameSnapshot& snapshot,
        PlayerColor playerColor) const = 0;
};

} // namespace kungfu
