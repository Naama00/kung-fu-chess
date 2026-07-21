// players/ClientConfig.hpp
#pragma once

#include <chrono>

namespace kungfu {

struct ClientConfig {
    // Network configuration
    static constexpr auto kHeartbeatInterval = std::chrono::seconds(5);
    static constexpr auto kMoveRetryCheckInterval = std::chrono::milliseconds(100);
    static constexpr auto kMoveRetryTimeout = std::chrono::milliseconds(200);
    static constexpr int kMaxMoveRetries = 5;
    static constexpr int kDefaultDisconnectCountdownSec = 20;
};

} // namespace kungfu