// src/server/main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "network/TcpServer.hpp"
#include "network/UdpServer.hpp"
#include "network/SessionManager.hpp"
#include "match/MatchManager.hpp"
#include "persistence/SqliteUserRepository.hpp"
#include "persistence/PasswordHasher.hpp"
#include "ServerConfig.hpp"

int main() {
    try {
        boost::asio::io_context ioContext;

        // Instantiate concrete Repository and Password Hasher implementations
        auto userRepository = std::make_shared<kungfu::SqliteUserRepository>();
        auto passwordHasher = std::make_shared<kungfu::SodiumPasswordHasher>();

        // Initialize SQLite persistence layer
        if (!userRepository->initialize("kungfu_chess.db")) {
            std::cerr << "Database initialization failed! Exiting." << std::endl;
            return 1;
        }

        // Inject interfaces via Dependency Injection
        kungfu::MatchManager matchManager(ioContext, userRepository, passwordHasher);
        kungfu::SessionManager sessionManager;

        std::cout << "Starting KungFu Chess Server..." << std::endl;

        // Two independent transports sharing one SessionManager
        kungfu::TcpServer tcpServer(ioContext, kungfu::ServerConfig::kTcpPort, matchManager, sessionManager);
        kungfu::UdpServer udpServer(ioContext, kungfu::ServerConfig::kUdpPort, sessionManager);

        std::cout << "Server is running. Waiting for players to connect..." << std::endl;

        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}