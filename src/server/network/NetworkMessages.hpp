#pragma once

#include <cstdint>
#include <cstddef>

namespace kungfu {

// Message types sent over the wire.
enum class NetworkMessageType : std::uint8_t {
    JOIN_MATCH_REQUEST,       // Client requests to join matchmaking
    MATCH_FOUND,              // Server announces a match was found; returns match id + color
    GAME_MOVE,                // A move (client -> server, or server -> both clients)
    MOVE_RESULT,              // Server returns the result of a move (legal/illegal)
    HEARTBEAT,                // Small keep-alive signal to maintain the connection
    GAME_OVER,                // Server announces both sides that the match ended naturally
    OPPONENT_DISCONNECTED,    // Server announces the opponent disconnected and the match was cancelled
    LOGIN_REQUEST,            // Client sends login credentials (text)
    LOGIN_RESPONSE,           // Server response: 0=failed, 1=success + 4-byte ELO rating
    REGISTER_REQUEST,         // Client requests to register (text)
    REGISTER_RESPONSE,        // Server response: 0=failed, 1=success
    MATCH_TIMEOUT,            // Server announces the match was cancelled due to a matchmaking timeout
    DISCONNECT_COUNTDOWN,     // Server announces the match is about to end due to opponent inactivity
    SPECTATE_ROOM_REQUEST,    // Client requests to spectate a specific match by ID
    ROOM_STATE_SYNC,          // Server sends the current board layout and match status to a spectator
    ROOM_LIST_REQUEST,        // Client requests a list of active matches
    ROOM_LIST_RESPONSE,       // Server returns active matches (IDs + Player Usernames)
};

// Compact wire representation of a board Position.
struct NetworkPosition {
    std::int32_t x;
    std::int32_t y;
};

// In-memory representation of a move packet.
// The wire format is serialized explicitly, so it does not depend on
// compiler-specific struct layout, padding, or endianness.
struct NetworkMovePacket {
    std::uint64_t matchId;
    std::uint64_t requestId;
    std::uint8_t  playerColor; // Converted from PlayerColor
    NetworkPosition from;
    NetworkPosition to;
};

// Fixed, explicit (not sizeof!) size of NetworkMovePacket as written on the wire:
// 8 (matchId) + 8 (requestId) + 1 (playerColor) + 4*4 (from.x/y, to.x/y) = 33 bytes
inline constexpr std::size_t kMovePacketWireSize = 8 + 8 + 1 + 4 * 4;

// Generic header for every network packet: 1 byte message type, followed by
// 4 bytes (Big-Endian) of payload length.
inline constexpr std::size_t kHeaderSize = 5;

// Guards against an attacker/bug trying to make the server allocate a huge
// payload buffer (DoS vector). 64KB is generous compared to any legitimate
// message in the current protocol.
inline constexpr std::uint32_t kMaxPayloadSize = 64 * 1024;

} // namespace kungfu