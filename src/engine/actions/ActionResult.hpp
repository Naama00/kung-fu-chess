// engine/actions/ActionResult.hpp
#pragma once

#include <cstdint>

namespace kungfu {

/// Result of processing an ActionRequest by the GameEngine.
enum class ActionStatus {
    Accepted,       // the request was accepted and executed
    Rejected,       // the request was rejected (for example, illegal move, piece in motion, piece in cooldown, invalid destination)
    StoredAsPending // the request was stored as a premove for future execution
};

// full result containing the status and the original request identifier.
//   - IllegalMove      : the move is illegal according to chess rules
//   - PieceMoving      : the piece is already in motion
//   - PieceCoolingDown : the piece is in cooldown
//   - InvalidTarget    : target position outside the board boundaries
struct ActionResult {
    std::uint64_t requestId; // original request identifier
    ActionStatus  status;

    ActionResult(std::uint64_t requestId, ActionStatus status)
        : requestId(requestId), status(status) {}
};

} // namespace kungfu
