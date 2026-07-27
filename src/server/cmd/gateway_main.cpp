// server/cmd/gateway_main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "server/network/TcpServer.hpp"
#include "server/network/UdpServer.hpp"
#include "server/network/SessionManager.hpp"
#include "server/network/RedisPubSubClient.hpp"
#include "server/match/MatchManager.hpp"
#include "server/match/RedisSessionRegistry.hpp"
#include "server/persistence/RedisUserRepository.hpp"
#include "server/persistence/PasswordHasher.hpp"
#include "server/ServerConfig.hpp"

namespace kungfu {

/**
 * @brief Standalone Gateway Entry Point.
 * 
 * Handles client authentication, session bindings, and acts as a lightweight proxy
 * forwarding realtime game actions (GAME_MOVE) to Authoritative Game Server Shards
 * via Redis Pub/Sub messaging.
 */
void runGatewayService() {
    boost::asio::io_context ioContext;

    std::uint16_t tcpPort = ServerConfig::getTcpPort();
    std::uint16_t udpPort = ServerConfig::getUdpPort();
    std::string redisHost = ServerConfig::getRedisHost();
    std::uint16_t redisPort = ServerConfig::getRedisPort();

    std::cout << "==================================================" << std::endl;
    std::cout << "   KUNG-FU CHESS: API & REALTIME GATEWAY SERVICE  " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Initialize Redis User Repository for Client Auth
    auto userRepository = std::make_shared<RedisUserRepository>();
    std::string redisConn = redisHost + ":" + std::to_string(redisPort);
    if (!userRepository->initialize(redisConn)) {
        std::cerr << "[Gateway] Critical: Redis User Repository init failed!" << std::endl;
        return;
    }

    // 2. Initialize Redis Session & Match Registry
    auto sessionRegistry = std::make_shared<RedisSessionRegistry>();
    sessionRegistry->initialize(redisHost, redisPort);

    // 3. Initialize Inter-Service Redis Pub/Sub Client
    auto pubSubClient = std::make_shared<RedisPubSubClient>(ioContext);
    pubSubClient->connectPublisher(redisHost, redisPort);

    auto passwordHasher = std::make_shared<SodiumPasswordHasher>();
    MatchManager matchManager(ioContext, userRepository, passwordHasher);
    SessionManager sessionManager;

    // 4. Subscribe Gateway to receive match updates/results broadcast by Shards
    pubSubClient->subscribe("gateway:broadcast", [&sessionManager](const std::string& channel, const std::string& payload) {
        (void)channel;
        // Payload format: "RESULT|sessionToken|wirePayload" or "MOVE|matchId|wirePayload"
        std::cout << "[Gateway PubSub] Broadcast received: " << payload << std::endl;
    }, redisHost, redisPort);

    // 5. Start TCP Control and UDP Realtime Listening Servers
    TcpServer tcpServer(ioContext, tcpPort, matchManager, sessionManager);
    UdpServer udpServer(ioContext, udpPort, sessionManager);

    std::cout << "[Gateway] Listening on TCP:" << tcpPort << " and UDP:" << udpPort << std::endl;
    std::cout << "[Gateway] Service ready and waiting for clients..." << std::endl;

    ioContext.run();
}

} // namespace kungfu

int main() {
    try {
        kungfu::runGatewayService();
    } catch (const std::exception& e) {
        std::cerr << "[Gateway] Fatal Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}