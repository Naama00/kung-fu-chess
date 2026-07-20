#include <catch2/catch_test_macros.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream> 
#include "server/network/NetworkServer.hpp"
#include "server/match/MatchManager.hpp"
#include "players/network/NetworkPlayer.hpp"

using namespace kungfu;

TEST_CASE("Asynchronous Network Matchmaking and Move Relay Integration Test", "[network]") {
    boost::asio::io_context serverIo;
    boost::asio::io_context clientIo;
    
    const std::uint16_t testPort = 8086;

    // 1. Define the game manager and the server lobby
    auto matchManager = std::make_shared<MatchManager>(serverIo);
    auto server = std::make_unique<NetworkServer>(serverIo, testPort, *matchManager);

    // 2. Run the server event loop in a separate thread
    std::thread serverThread([&]() {
        serverIo.run();
    });

    // 3. הרצת לולאת האירועים של הקליינטים ב-thread נפרד
    std::thread clientThread([&]() {
        boost::asio::io_context::work work(clientIo);
        clientIo.run();
    });

    // 4. Initialize two independent network players
    auto player1 = std::make_shared<NetworkPlayer>(clientIo, "127.0.0.1", "8086");
    auto player2 = std::make_shared<NetworkPlayer>(clientIo, "127.0.0.1", "8086");

    // 5. Connect to the server asynchronously (send a JOIN request)
    player1->connectAndJoin();
    player2->connectAndJoin();

    // Smart, dynamic wait of up to 3 seconds for matchmaking to finish on the server
    int retries = 30;
    while (retries > 0 && player1->matchId() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries--;
    }

    // Verify that the clients are connected properly
    REQUIRE(player1->isConnected() == true);
    REQUIRE(player2->isConnected() == true);

    // Verify that matchmaking succeeded and they were assigned to the same game
    REQUIRE(player1->matchId() == player2->matchId());
    REQUIRE(player1->matchId() != 0);
    
    // Verify that the server assigned them different colors
    REQUIRE(player1->assignedColor() != player2->assignedColor());

    // 6. Send a test move from player 1 (e2 -> e4)
    PlayerAction testAction({6, 4}, {4, 4});
    player1->sendMoveToServer(testAction);

   // Smart, dynamic wait of up to 3 seconds for matchmaking to finish on the server
    retries = 30;
    while (retries > 0 && player1->matchId() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries--;
    }

    // Sample the moves received by player 2
    view::GameSnapshot dummySnapshot{};
    auto receivedActions = player2->decideActions(dummySnapshot);

    // Fully verify that player 2 received exactly the same move that the other player made!
    REQUIRE(receivedActions.size() == 1);
    REQUIRE(receivedActions[0].action.from == testAction.from);
    REQUIRE(receivedActions[0].action.to == testAction.to);

    std::cout << "[Catch2 Test] Matchmaking and move relay verified successfully!" << std::endl;

    // 7. Atomically clean up and stop the loops to end the test cleanly
    serverIo.stop();
    clientIo.stop();

    if (serverThread.joinable()) serverThread.join();
    if (clientThread.joinable()) clientThread.join();
}