// players/IPlayer.hpp
#pragma once

#include <vector>
#include "engine/actions/ActionRequest.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace kungfu {

/// @brief Common interface for all player types in the game.
///
/// Every implementation receives a read-only snapshot (GameSnapshot) and returns
/// a collection of action requests (ActionRequest) that it wants to execute.
///
/// @note Future implementations — HumanPlayer, AIPlayer, NetworkPlayer,
///       ReplayPlayer — are not allowed to access the Board or GameEngine directly.
///       The only write channel is returning ActionRequest from this method.
///       The GameEngine is responsible for validating and executing each request separately.
class IPlayer {
public:
    virtual ~IPlayer() = default;

    /// @brief Computes and returns the desired actions based on the current game state.
    /// @param snapshot A current game snapshot (read-only)
    /// @return A collection of action requests; it can be empty if there is no desired action at the moment
    virtual std::vector<ActionRequest> decideActions(const view::GameSnapshot& snapshot) = 0;
};

} // namespace kungfu
