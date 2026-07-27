// server/cmd/matchmaker_main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <chrono>
#include "server/match/DistributedMatchmaker.hpp"
#include "server/match/RedisSessionRegistry.hpp"
#include "server/match/GameAllocator.hpp"
#include "server/network/RedisPubSubClient.hpp"
#include "server/ServerConfig.hpp"

namespace kungfu {

/**
 * @brief Standalone Matchmaker & Game Allocator Service.
 * 
 * Periodically polls the central Redis matchmaking queue, pairs matched players,
 * delegates room provisioning to the GameAllocator, and notifies Gateway services
 * via Redis Pub/Sub messaging.
 */
class MatchmakerDaemon {
private:
    boost::asio::io_context& m_ioContext;
    boost::asio::steady_timer m_timer;

    std::shared_ptr<DistributedMatchmaker> m_matchmaker;
    std::shared_ptr<RedisSessionRegistry> m_sessionRegistry;
    std::shared_ptr<RedisPubSubClient> m_pubSubClient;
    std::unique_ptr<GameAllocator> m_allocator;

    std::uint64_t m_nextMatchId{1000};

    void schedulePollingCycle() {
        m_timer.expires_after(ServerConfig::kMatchmakingTickInterval);
        m_timer.async_wait([this](const boost::system::error_code& ec) {
            if (!ec) {
                runMatchmakingStep();
                schedulePollingCycle();
            }
        });
    }

    void runMatchmakingStep() {
        // Poll compatible matched pairs from global Redis set
        auto matchedPairs = m_matchmaker->pollMatchedPairs();

        for (const auto& pair : matchedPairs) {
            std::uint64_t matchId = m_nextMatchId++;

            std::cout << "[MatchmakerDaemon] Matched: " << pair.player1 
                      << " vs " << pair.player2 << " (Match ID: " << matchId << ")" << std::endl;

            // Provision room on an available Game Server Shard
            std::string assignedShard = m_allocator->allocateMatch(matchId, pair.player1, pair.player2);

            if (!assignedShard.empty()) {
                // Notify Gateway to inform both clients via MATCH_FOUND payload
                std::ostringstream gatewaySignal;
                gatewaySignal << "MATCH_FOUND|" << matchId << "|" << pair.player1 << "|" << pair.player2 << "|" << assignedShard;
                m_pubSubClient->publish("gateway:broadcast", gatewaySignal.str());
            }
        }
    }

public:
    MatchmakerDaemon(boost::asio::io_context& ioContext, const std::string& redisHost, std::uint16_t redisPort)
        : m_ioContext(ioContext),
          m_timer(ioContext) {

        m_matchmaker = std::make_shared<DistributedMatchmaker>();
        m_matchmaker->initialize(redisHost, redisPort);

        m_sessionRegistry = std::make_shared<RedisSessionRegistry>();
        m_sessionRegistry->initialize(redisHost, redisPort);

        m_pubSubClient = std::make_shared<RedisPubSubClient>(ioContext);
        m_pubSubClient->connectPublisher(redisHost, redisPort);

        m_allocator = std::make_unique<GameAllocator>(m_sessionRegistry, m_pubSubClient);

        // Register default initial game server shards (can be updated dynamically via Redis)
        m_allocator->registerShard("gameserver-1");
        m_allocator->registerShard("gameserver-2");

        schedulePollingCycle();
    }
};

} // namespace kungfu

int main() {
    try {
        boost::asio::io_context ioContext;

        std::string redisHost = kungfu::ServerConfig::getRedisHost();
        std::uint16_t redisPort = kungfu::ServerConfig::getRedisPort();

        std::cout << "==================================================" << std::endl;
        std::cout << "   KUNG-FU CHESS: MATCHMAKER & ALLOCATOR SERVICE  " << std::endl;
        std::cout << "==================================================" << std::endl;

        kungfu::MatchmakerDaemon daemon(ioContext, redisHost, redisPort);

        std::cout << "[Matchmaker] Polling Redis queues for player pairings..." << std::endl;
        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "[Matchmaker] Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}