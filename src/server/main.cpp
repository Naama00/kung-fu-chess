#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "NetworkServer.hpp"
#include "MatchManager.hpp"

int main() {
    try {
        const std::uint16_t port = 8080;
        boost::asio::io_context ioContext;

        // העברת ה-ioContext כדי שה-MatchManager יוכל להגדיר טיימרים לכל משחק חצי
        kungfu::MatchManager matchManager(ioContext);

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