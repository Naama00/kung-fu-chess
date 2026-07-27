// server/main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "network/TcpServer.hpp"
#include "network/UdpServer.hpp"
#include "network/SessionManager.hpp"
#include "match/MatchManager.hpp"
#include "persistence/SqliteUserRepository.hpp"
#include "persistence/RedisUserRepository.hpp"
#include "persistence/PasswordHasher.hpp"
#include "ServerConfig.hpp"

int main() {
    try {
        boost::asio::io_context ioContext;

        std::uint16_t tcpPort = kungfu::ServerConfig::getTcpPort();
        std::uint16_t udpPort = kungfu::ServerConfig::getUdpPort();
        std::string dbType = kungfu::ServerConfig::getDbType();

        std::shared_ptr<kungfu::IUserRepository> userRepository;

        if (dbType == "redis") {
            std::string redisConn = kungfu::ServerConfig::getRedisHost() + ":" + 
                                    std::to_string(kungfu::ServerConfig::getRedisPort());
            userRepository = std::make_shared<kungfu::RedisUserRepository>();
            if (!userRepository->initialize(redisConn)) {
                std::cerr << "Redis Repository initialization failed! Exiting." << std::endl;
                return 1;
            }
        } else {
            std::string dbPath = kungfu::ServerConfig::getDbPath();
            userRepository = std::make_shared<kungfu::SqliteUserRepository>();
            if (!userRepository->initialize(dbPath)) {
                std::cerr << "SQLite Repository initialization failed! Exiting." << std::endl;
                return 1;
            }
        }

        auto passwordHasher = std::make_shared<kungfu::SodiumPasswordHasher>();

        kungfu::MatchManager matchManager(ioContext, userRepository, passwordHasher);
        kungfu::SessionManager sessionManager;

        std::cout << "Starting KungFu Chess Server..." << std::endl;
        std::cout << "[Config] Database Engine: " << dbType << std::endl;

        kungfu::TcpServer tcpServer(ioContext, tcpPort, matchManager, sessionManager);
        kungfu::UdpServer udpServer(ioContext, udpPort, sessionManager);

        std::cout << "Server is running. Waiting for players to connect..." << std::endl;

        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}