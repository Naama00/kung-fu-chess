#pragma once

#include <chrono>

namespace kungfu {

struct ServerConfig {
    static constexpr int kDefaultRating = 1200;

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