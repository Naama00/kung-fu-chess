// engine/snapshot/MatchStateSnapshot.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "engine/common/Enums.hpp"
#include "engine/common/Position.hpp"

namespace kungfu {

// Represents an in-flight piece motion with relative elapsed and total duration
struct MotionSnapshot {
    std::uint64_t pieceId = 0;
    Position from;
    Position to;
    int elapsedMs = 0;  // Milliseconds elapsed since motion start
    int durationMs = 0; // Total planned motion duration
};

// Represents an active cooldown with remaining duration
struct CooldownSnapshot {
    std::uint64_t pieceId = 0;
    int remainingMs = 0; // Milliseconds remaining until cooldown expires
};

// Represents a pending premove target for a specific piece
struct PremoveSnapshot {
    std::uint64_t pieceId = 0;
    Position to;
};

// Represents the snapshot of an individual piece
struct PieceStateSnapshot {
    std::uint64_t id = 0;
    PieceType type = PieceType::Pawn;
    PlayerColor color = PlayerColor::White;
    Position position;
    PieceState state = PieceState::Idle;
    bool hasMoved = false;
};

// Comprehensive snapshot container for a live match used in state persistence and failover
struct MatchStateSnapshot {
    std::uint64_t matchId = 0;
    int currentTimeMs = 0;
    bool gameOver = false;
    PlayerColor currentTurn = PlayerColor::White;

    std::string whiteUsername;
    std::string blackUsername;
    bool isWhiteDisconnected = false;
    bool isBlackDisconnected = false;
    int reconnectSecondsLeft = 0;

    std::vector<PieceStateSnapshot> pieces;
    std::vector<MotionSnapshot> activeMotions;
    std::vector<CooldownSnapshot> activeCooldowns;
    std::vector<PremoveSnapshot> pendingPremoves;
};

} // namespace kungfu