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

    // 1. הגדרת מנהל המשחקים והשוער בשרת
    auto matchManager = std::make_shared<MatchManager>(serverIo);
    auto server = std::make_unique<NetworkServer>(serverIo, testPort, *matchManager);

    // 2. הרצת לולאת האירועים של השרת ב-thread נפרד
    std::thread serverThread([&]() {
        serverIo.run();
    });

    // 3. הרצת לולאת האירועים של הקליינטים ב-thread נפרד
    std::thread clientThread([&]() {
        boost::asio::io_context::work work(clientIo);
        clientIo.run();
    });

    // 4. אתחול של שני שחקני רשת עצמאיים
    auto player1 = std::make_shared<NetworkPlayer>(clientIo, "127.0.0.1", "8086");
    auto player2 = std::make_shared<NetworkPlayer>(clientIo, "127.0.0.1", "8086");

    // 5. התחברות לשרת בצורה אסינכרונית (שיגור בקשת JOIN)
    player1->connectAndJoin();
    player2->connectAndJoin();

    // המתנה חכמה ודינמית של עד 3 שניות לסיום השידוך בשרת
    int retries = 30;
    while (retries > 0 && player1->matchId() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries--;
    }

    // וידוא שהלקוחות מחוברים היטב
    REQUIRE(player1->isConnected() == true);
    REQUIRE(player2->isConnected() == true);

    // וידוא שהשידוך הצליח והם שויכו לאותו משחק
    REQUIRE(player1->matchId() == player2->matchId());
    REQUIRE(player1->matchId() != 0);
    
    // וידוא שהשרת הקצה להם צבעים שונים
    REQUIRE(player1->assignedColor() != player2->assignedColor());

    // 6. שליחת מהלך בדיקה משחקן 1 (e2 -> e4)
    PlayerAction testAction({6, 4}, {4, 4});
    player1->sendMoveToServer(testAction);

   // המתנה חכמה ודינמית של עד 3 שניות לסיום השידוך בשרת
    retries = 30;
    while (retries > 0 && player1->matchId() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries--;
    }

    // דגימת המהלכים שהתקבלו אצל שחקן 2
    view::GameSnapshot dummySnapshot{};
    auto receivedActions = player2->decideActions(dummySnapshot);

    // וידוא מלא ששחקן 2 קיבל בדיוק את אותו המהלך שהשני ביצע!
    REQUIRE(receivedActions.size() == 1);
    REQUIRE(receivedActions[0].action.from == testAction.from);
    REQUIRE(receivedActions[0].action.to == testAction.to);

    std::cout << "[Catch2 Test] Matchmaking and move relay verified successfully!" << std::endl;

    // 7. ניקוי ועצירה אטומיים של הלולאות לטובת סיום הטסט בצורה נקייה
    serverIo.stop();
    clientIo.stop();

    if (serverThread.joinable()) serverThread.join();
    if (clientThread.joinable()) clientThread.join();
}