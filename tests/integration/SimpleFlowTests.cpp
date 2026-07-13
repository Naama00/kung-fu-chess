#include <catch2/catch_test_macros.hpp>
#include "io/BoardParser.hpp"
#include "io/BoardPrinter.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"
#include "input/Controller.hpp"

TEST_CASE("Full System Integration Flow", "[integration]") {
    std::string setup = 
        "wR . .\n"
        ". . .\n"
        ". . bK\n"; // צריח ב-(0,0), מלך שחור ב-(2,2)

    auto board = kungfu::BoardParser::parse(setup);
    REQUIRE(board != nullptr);

    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    auto gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine);
    kungfu::Controller controller(gameEngine, 100);

    SECTION("Click to move and await arrival") {
        // קליק ראשון - בחירת הצריח ב-(0,0) (מיוצג ע"י פיקסלים 50, 50)
        auto click1 = controller.click(50, 50);
        REQUIRE(click1.actionTaken);
        REQUIRE(click1.description == "Piece selected");

        // קליק שני - הזמנת מהלך ליעד (0,2) (מיוצג ע"י פיקסלים 250, 50)
        // מרחק: 2 משבצות = 2,000 מילישניות
        auto click2 = controller.click(250, 50);
        REQUIRE(click2.actionTaken);
        REQUIRE(click2.description == "Move requested: ok");

        // המתנה ל-2000ms - הכלי הגיע ומצב הלוח מתעדכן
        gameEngine->wait(2000);
        std::string expectedAfterArrival = 
            ". . wR\n"
            ". . .\n"
            ". . bK\n";
        REQUIRE(kungfu::BoardPrinter::print(*board) == expectedAfterArrival);
    }
}