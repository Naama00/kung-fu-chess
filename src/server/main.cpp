// src/server/main.cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "network/NetworkServer.hpp"
#include "match/MatchManager.hpp"

int main() {
    try {
        const std::uint16_t port = 8080;
        boost::asio::io_context ioContext;

        kungfu::MatchManager matchManager(ioContext);

        // SQLite DB Initialization using secure DatabaseManager configuration
        if (!matchManager.dbManager().initialize("kungfu_chess.db")) {
            std::cerr << "Database initialization failed! Exiting." << std::endl;
            return 1;
        }

        std::cout << "Starting KungFu Chess Server on port " << port << "..." << std::endl;
        kungfu::NetworkServer server(ioContext, port, matchManager);

        std::cout << "Server is running. Waiting for players to connect..." << std::endl;

        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}