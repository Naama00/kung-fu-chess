// Role: abstract interface defining how to evaluate how good the current board state is for a specific player 
// (return a numeric score).
#pragma once

#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

class IPositionEvaluator {
public:
    virtual ~IPositionEvaluator() = default;

    virtual int evaluate(const view::GameSnapshot& snapshot, PlayerColor playerColor) const = 0;
};

} // namespace kungfu