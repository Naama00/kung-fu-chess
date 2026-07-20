// players/ai/IAIDecisionStrategy.hpp
#pragma once

#include <vector>
#include "engine/actions/ActionRequest.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace kungfu {

class IAIDecisionStrategy {
public:
    virtual ~IAIDecisionStrategy() = default;

    // Computes and returns a list of preferred moves based on the board state
    virtual std::vector<ActionRequest> computeActions(
        const view::GameSnapshot& snapshot, 
        PlayerColor aiColor) = 0;
};

} // namespace kungfu