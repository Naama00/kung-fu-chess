// engine/actions/PlayerAction.hpp
#pragma once

#include "engine/common/Position.hpp"

namespace kungfu {

// Represents only a player intent; it does not change the game state.
// PlayerAction describes a request to move a piece from a source position to a target position.
// validation (legality, cooldown, authority) is performed only by the GameEngine
// when receiving the ActionRequest that wraps this action.
struct PlayerAction {
    Position from; // piece position when the request is submitted
    Position to;   // requested destination position

    PlayerAction(Position from, Position to)
        : from(from), to(to) {}
};

} // namespace kungfu
