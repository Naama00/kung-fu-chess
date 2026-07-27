// server/match/GameAllocator.cpp
#include "server/match/GameAllocator.hpp"
#include "server/match/RedisSessionRegistry.hpp"
#include "server/network/RedisPubSubClient.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace kungfu {

GameAllocator::GameAllocator(std::shared_ptr<RedisSessionRegistry> sessionRegistry,
                             std::shared_ptr<RedisPubSubClient> pubSubClient)
    : m_sessionRegistry(std::move(sessionRegistry)),
      m_pubSubClient(std::move(pubSubClient)) {}

void GameAllocator::registerShard(const std::string& shardId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_activeShards.begin(), m_activeShards.end(), shardId) == m_activeShards.end()) {
        m_activeShards.push_back(shardId);
        std::cout << "[GameAllocator] Registered new shard: " << shardId 
                  << " (Total active shards: " << m_activeShards.size() << ")" << std::endl;
    }
}

std::string GameAllocator::allocateMatch(std::uint64_t matchId,
                                         const std::string& whiteUsername,
                                         const std::string& blackUsername) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeShards.empty()) {
        std::cerr << "[GameAllocator] Error: No active shards available to host match " << matchId << std::endl;
        return "";
    }

    // Round-Robin selection algorithm
    std::size_t selectedIdx = m_rrIndex.fetch_add(1) % m_activeShards.size();
    std::string targetShard = m_activeShards[selectedIdx];

    // 1. Record mapping in Redis Session Registry
    if (m_sessionRegistry) {
        m_sessionRegistry->registerMatch(matchId, targetShard, whiteUsername, blackUsername);
    }

    // 2. Dispatch provisioning payload to the target shard's control topic
    if (m_pubSubClient) {
        std::ostringstream payload;
        payload << "PROVISION|" << matchId << "|" << whiteUsername << "|" << blackUsername;
        
        std::string shardChannel = "shard:" + targetShard + ":commands";
        m_pubSubClient->publish(shardChannel, payload.str());

        std::cout << "[GameAllocator] Provisioned match " << matchId << " on " 
                  << targetShard << " via Pub/Sub topic: " << shardChannel << std::endl;
    }

    return targetShard;
}

} // namespace kungfu