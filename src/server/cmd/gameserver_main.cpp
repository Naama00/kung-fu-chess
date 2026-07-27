// server/cmd/gameserver_main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <cstdlib>
#include "server/match/LiveMatch.hpp"
#include "server/match/MatchFactory.hpp"
#include "server/network/RedisPubSubClient.hpp"
#include "server/ServerConfig.hpp"

namespace kungfu {

/**
 * @class GameServerShard
 * @brief Authoritative Game Server Shard runtime executable.
 * 
 * Runs isolated LiveMatch instances, executes game ticks, processes player moves received
 * via Redis Pub/Sub, and broadcasts authoritative state updates back to Gateway services.
 */
class GameServerShard : public std::enable_shared_from_this<GameServerShard> {
private:
    boost::asio::io_context& m_ioContext;
    std::string m_shardId;
    std::shared_ptr<RedisPubSubClient> m_pubSubClient;

    // Active live matches hosted on this specific shard node
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_activeMatches;
    std::mutex m_mutex;

    void handleProvisionCommand(const std::string& payload) {
        // Payload format: "PROVISION|matchId|whiteUser|blackUser"
        std::stringstream ss(payload);
        std::string tag, matchIdStr, whiteUser, blackUser;

        std::getline(ss, tag, '|');
        std::getline(ss, matchIdStr, '|');
        std::getline(ss, whiteUser, '|');
        std::getline(ss, blackUser, '|');

        if (tag == "PROVISION" && !matchIdStr.empty()) {
            std::uint64_t matchId = std::stoull(matchIdStr);

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_activeMatches.find(matchId) != m_activeMatches.end()) {
                return; // Already hosted
            }

            // Construct new authoritative match using MatchFactory
            auto match = MatchFactory::createStandardMatch(m_ioContext, matchId, nullptr, nullptr);
            
            // Set match completion cleanup hook
            match->setOnMatchEnded([this](std::uint64_t finishedId) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_activeMatches.erase(finishedId);
                std::cout << "[" << m_shardId << "] Match " << finishedId << " completed and freed." << std::endl;
            });

            m_activeMatches[matchId] = match;
            match->start(); // Starts Asio tick loop

            std::cout << "[" << m_shardId << "] Successfully provisioned live match " << matchId 
                      << " (" << whiteUser << " vs " << blackUser << ")" << std::endl;
        }
    }

public:
    GameServerShard(boost::asio::io_context& ioContext, std::string shardId, const std::string& redisHost, std::uint16_t redisPort)
        : m_ioContext(ioContext), m_shardId(std::move(shardId)) {

        m_pubSubClient = std::make_shared<RedisPubSubClient>(ioContext);
        m_pubSubClient->connectPublisher(redisHost, redisPort);

        // Subscribe to shard-specific control channel
        std::string commandChannel = "shard:" + m_shardId + ":commands";
        m_pubSubClient->subscribe(commandChannel, [this](const std::string& channel, const std::string& payload) {
            (void)channel;
            handleProvisionCommand(payload);
        }, redisHost, redisPort);
    }
};

} // namespace kungfu

int main(int argc, char* argv[]) {
    try {
        boost::asio::io_context ioContext;

        // Obtain shard identity from command-line argument or environment variable
        std::string shardId = (argc > 1) ? argv[1] : "gameserver-1";
        if (const char* envShard = std::getenv("KUNGFU_SHARD_ID")) {
            shardId = envShard;
        }

        std::string redisHost = kungfu::ServerConfig::getRedisHost();
        std::uint16_t redisPort = kungfu::ServerConfig::getRedisPort();

        std::cout << "==================================================" << std::endl;
        std::cout << "   KUNG-FU CHESS: GAME SERVER SHARD [" << shardId << "]" << std::endl;
        std::cout << "==================================================" << std::endl;

        auto shard = std::make_shared<kungfu::GameServerShard>(ioContext, shardId, redisHost, redisPort);

        std::cout << "[" << shardId << "] Authoritative Game Engine Shard is running..." << std::endl;
        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "[GameServerShard] Fatal Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}