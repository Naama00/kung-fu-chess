// src/server/main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "network/TcpServer.hpp"
#include "network/UdpServer.hpp"
#include "network/SessionManager.hpp"
#include "match/MatchManager.hpp"
#include "ServerConfig.hpp"

int main() {
    try {
        boost::asio::io_context ioContext;

        kungfu::MatchManager matchManager(ioContext);
        kungfu::SessionManager sessionManager;

        // SQLite DB Initialization using secure DatabaseManager configuration
        if (!matchManager.dbManager().initialize("kungfu_chess.db")) {
            std::cerr << "Database initialization failed! Exiting." << std::endl;
            return 1;
        }

        std::cout << "Starting KungFu Chess Server..." << std::endl;

        // Two independent transports, sharing one SessionManager so a
        // client's TCP (control) and UDP (realtime) identities can be
        // correlated. See network/NetworkMessages.hpp for the full
        // protocol split and the SESSION_BIND handshake.
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
