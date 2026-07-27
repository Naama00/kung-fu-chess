// server/match/GameAllocator.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>
#include <atomic>

namespace kungfu {

class RedisSessionRegistry;
class RedisPubSubClient;

/**
 * @class GameAllocator
 * @brief Manages the distribution of live rooms across available Game Server Shards.
 *
 * Implements load balancing algorithms (e.g., Round-Robin), persists room-to-shard mapping
 * in Redis, and dispatches initialization signals to target shards via Pub/Sub.
 */
class GameAllocator {
private:
    std::shared_ptr<RedisSessionRegistry> m_sessionRegistry;
    std::shared_ptr<RedisPubSubClient> m_pubSubClient;

    std::vector<std::string> m_activeShards;
    std::atomic<std::size_t> m_rrIndex{0};
    mutable std::mutex m_mutex;

public:
    GameAllocator(std::shared_ptr<RedisSessionRegistry> sessionRegistry,
                  std::shared_ptr<RedisPubSubClient> pubSubClient);

    /**
     * @brief Registers a live Game Server Shard ID available for match hosting.
     * @param shardId Identifier string (e.g., "gameserver-1", "gameserver-2").
     */
    void registerShard(const std::string& shardId);

    /**
     * @brief Selects an optimal shard for a new match and provisions it.
     * @param matchId Unique 64-bit match identifier.
     * @param whiteUsername Username of White player.
     * @param blackUsername Username of Black player.
     * @return Assigned shard ID string, or empty string if no shards available.
     */
    std::string allocateMatch(std::uint64_t matchId,
                              const std::string& whiteUsername,
                              const std::string& blackUsername);
};

} // namespace kungfu