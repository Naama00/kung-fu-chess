#pragma once

#include <chrono>
#include <cstdint>

namespace kungfu {

struct ServerConfig {
    static constexpr int kDefaultRating = 1200;

    // TCP and UDP are independent port namespaces at the OS level, so both
    // servers can safely listen on the same numeric port.
    static constexpr std::uint16_t kTcpPort = 8080; // Control plane: auth, lobby, match lifecycle
    static constexpr std::uint16_t kUdpPort = 8080; // Realtime plane: moves, heartbeat

    // How long a bound UDP endpoint may stay silent before SessionManager
    // considers the realtime channel stale and unbinds it. This does NOT
    // disconnect the player - the TCP control channel remains the sole
    // authority on whether a player is actually connected (see
    // PlayerSession::handleDisconnect).
    static constexpr auto kSessionTimeout = std::chrono::seconds(25);
    static constexpr auto kSessionPruneInterval = std::chrono::seconds(5);

    // Matchmaking configuration
    static constexpr auto kMatchmakingTickInterval = std::chrono::seconds(1);
    static constexpr auto kMatchmakingTimeout = std::chrono::seconds(60);
    static constexpr int kBaseEloDiff = 100;
    static constexpr int kEloDiffExpansionPerSec = 10;

    // Reconnect countdown configuration
    static constexpr int kReconnectTimeoutSec = 20;
    static constexpr auto kReconnectTickInterval = std::chrono::seconds(1);
};

} // namespace kungfu
