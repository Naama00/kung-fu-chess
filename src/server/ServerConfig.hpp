// server/ServerConfig.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace kungfu
{

    struct ServerConfig
    {
        static constexpr int kDefaultRating = 1200;

        static constexpr std::uint16_t kTcpPort = 8080;
        static constexpr std::uint16_t kUdpPort = 8080;

        static constexpr auto kSessionTimeout = std::chrono::seconds(25);
        static constexpr auto kSessionPruneInterval = std::chrono::seconds(5);

        // Live Match configuration
        static constexpr auto kLiveMatchTickInterval = std::chrono::milliseconds(50);

        // Matchmaking configuration
        static constexpr auto kMatchmakingTickInterval = std::chrono::seconds(1);
        static constexpr auto kMatchmakingTimeout = std::chrono::seconds(60);
        static constexpr int kBaseEloDiff = 100;
        static constexpr int kEloDiffExpansionPerSec = 10;

        // Reconnect countdown configuration
        static constexpr int kReconnectTimeoutSec = 20;
        static constexpr auto kReconnectTickInterval = std::chrono::seconds(1);

        // Dynamic configuration helpers reading environment variables with fallback
        static std::uint16_t getTcpPort()
        {
            if (const char *env = std::getenv("KUNGFU_TCP_PORT"))
            {
                return static_cast<std::uint16_t>(std::atoi(env));
            }
            return kTcpPort;
        }

        static std::uint16_t getUdpPort()
        {
            if (const char *env = std::getenv("KUNGFU_UDP_PORT"))
            {
                return static_cast<std::uint16_t>(std::atoi(env));
            }
            return kUdpPort;
        }

        static std::string getDbPath()
        {
            if (const char *env = std::getenv("KUNGFU_DB_PATH"))
            {
                return std::string(env);
            }
            return "kungfu_chess.db";
        }

        static std::string getDbType()
        {
            if (const char *env = std::getenv("KUNGFU_DB_TYPE"))
            {
                return std::string(env);
            }
            return "sqlite"; // Default fallback
        }

        static std::string getRedisHost()
        {
            if (const char *env = std::getenv("KUNGFU_REDIS_HOST"))
            {
                return std::string(env);
            }
            return "127.0.0.1";
        }

        static std::uint16_t getRedisPort()
        {
            if (const char *env = std::getenv("KUNGFU_REDIS_PORT"))
            {
                return static_cast<std::uint16_t>(std::atoi(env));
            }
            return 6379;
        }

        static std::string getPostgresConn()
        {
            if (const char *env = std::getenv("KUNGFU_POSTGRES_CONN"))
            {
                return std::string(env);
            }
            return "host=127.0.0.1 port=5432 dbname=kungfu user=postgres password=postgres";
        }
    };

} // namespace kungfu