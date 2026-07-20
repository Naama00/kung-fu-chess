// engine/actions/ActionRequest.hpp
#pragma once

#include <cstdint>
#include "engine/actions/PlayerAction.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

// Wraps PlayerAction with identity context: who sent it and what request it is.
// requestId is intended for tracking and debugging — it allows linking a request to its result
// even when multiple players submit actions simultaneously.
struct ActionRequest {
    std::uint64_t requestId;    // unique request identifier (for tracking/debugging)
    PlayerColor   playerColor;  // color of the player who submitted the request
    PlayerAction  action;       // requested action

    ActionRequest(std::uint64_t requestId, PlayerColor playerColor, PlayerAction action)
        : requestId(requestId), playerColor(playerColor), action(action) {}
};

} // namespace kungfu
