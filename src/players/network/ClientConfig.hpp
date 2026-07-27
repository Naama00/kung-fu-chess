// players/network/ClientConfig.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace kungfu {

struct ClientConfig {
    // Default server connectivity parameters
    static constexpr const char* kDefaultHost = "127.0.0.1";
    static constexpr const char* kDefaultPort = "8080";

    static constexpr auto kHeartbeatInterval = std::chrono::seconds(5);
    static constexpr auto kMoveRetryCheckInterval = std::chrono::milliseconds(100);
    static constexpr auto kMoveRetryTimeout = std::chrono::milliseconds(200);
    static constexpr int kMaxMoveRetries = 5;
    static constexpr int kDefaultDisconnectCountdownSec = 20;
    static constexpr std::uint32_t kDefaultRating = 1200; 

    // Dynamic configuration helpers for environment-driven network client setup
    static std::string getHost() {
        if (const char* env = std::getenv("KUNGFU_SERVER_HOST")) {
            return std::string(env);
        }
        return kDefaultHost;
    }

    static std::string getPort() {
        if (const char* env = std::getenv("KUNGFU_SERVER_PORT")) {
            return std::string(env);
        }
        return kDefaultPort;
    }
};

} // namespace kungfu