#pragma once

#include <cstdint>
#include <cstddef>

namespace kungfu {

// ============================================================================
// Wire protocol contract, shared by both transports.
//
// KungFu Chess uses two independent transports:
//   - TCP ("control channel"):  reliable, ordered. Carries authentication,
//     lobby/matchmaking, room listing, spectating, and match-lifecycle
//     notifications (game over, disconnects, timeouts).
//   - UDP ("realtime channel"): low-latency, best-effort. Carries in-match
//     moves, move results, and heartbeats.
//
// Every message type below belongs to exactly one channel. channelFor() is
// the single source of truth for that mapping: PlayerSession::sendPacket()
// uses it to route outgoing messages automatically, and TcpServer/UdpServer
// use it to reject messages that arrive on the wrong transport.
//
// Handshake sequence for a new client:
//   1. Open a TCP connection and send LOGIN_REQUEST.
//   2. On success, LOGIN_RESPONSE carries a one-time sessionToken.
//   3. Send SESSION_BIND{sessionToken} over UDP to attach the realtime
//      channel to that same logical session; server replies SESSION_BIND_ACK.
//   4. Proceed with lobby actions over TCP and gameplay over UDP.
// ============================================================================

enum class NetworkMessageType : std::uint8_t {
    JOIN_MATCH_REQUEST,      // TCP. Client requests to join matchmaking
    MATCH_FOUND,             // TCP. Server announces a match was found; returns match id + color
    GAME_MOVE,               // UDP. A move (client -> server, or server -> both clients)
    MOVE_RESULT,             // UDP. Server returns the result of a move (legal/illegal)
    HEARTBEAT,               // UDP. Keep-alive for the realtime channel (NAT keepalive / liveness)
    GAME_OVER,               // TCP. Server announces both sides that the match ended naturally
    OPPONENT_DISCONNECTED,   // TCP. Server announces the opponent disconnected and the match was cancelled
    LOGIN_REQUEST,           // TCP. Client sends login credentials (text)
    LOGIN_RESPONSE,          // TCP. success flag + [ELO rating + sessionToken] if successful
    REGISTER_REQUEST,        // TCP. Client requests to register (text)
    REGISTER_RESPONSE,       // TCP. Server response: 0=failed, 1=success
    MATCH_TIMEOUT,           // TCP. Server announces the match was cancelled due to a matchmaking timeout
    DISCONNECT_COUNTDOWN,    // TCP. Server announces the match is about to end due to opponent inactivity
    SPECTATE_ROOM_REQUEST,   // TCP. Client requests to spectate a specific match by ID
    ROOM_STATE_SYNC,         // TCP. Server sends the current board layout and match status to a spectator
    ROOM_LIST_REQUEST,       // TCP. Client requests a list of active matches
    ROOM_LIST_RESPONSE,      // TCP. Server returns active matches (IDs + Player Usernames)
    SESSION_BIND,            // UDP. Client attaches this UDP endpoint to an authenticated session
    SESSION_BIND_ACK,        // UDP. Server confirms the realtime channel is bound
};

// Which physical transport a message type is carried on.
enum class TransportChannel : std::uint8_t {
    Control,   // Reliable, ordered - carried over TCP
    Realtime,  // Low-latency, best-effort - carried over UDP
};

// Single source of truth for which transport carries each message type.
// See the protocol overview above for the full rationale.
constexpr TransportChannel channelFor(NetworkMessageType type) {
    switch (type) {
        case NetworkMessageType::GAME_MOVE:
        case NetworkMessageType::MOVE_RESULT:
        case NetworkMessageType::HEARTBEAT:
        case NetworkMessageType::SESSION_BIND:
        case NetworkMessageType::SESSION_BIND_ACK:
            return TransportChannel::Realtime;
        default:
            return TransportChannel::Control;
    }
}

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
// 4 bytes (Big-Endian) of payload length. Identical for both transports -
// UDP relies on it purely for validation, TCP relies on it for framing.
inline constexpr std::size_t kHeaderSize = 5;

// Guards against an attacker/bug trying to make the server allocate a huge
// payload buffer (DoS vector). 64KB is generous compared to any legitimate
// message in the current protocol.
inline constexpr std::uint32_t kMaxPayloadSize = 64 * 1024;

} // namespace kungfu
