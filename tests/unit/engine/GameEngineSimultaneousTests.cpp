#include <catch2/catch_test_macros.hpp>
#include "board/Board.hpp"
#include "io/BoardParser.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"

TEST_CASE("Simultaneous Mode Allows Both Colors Freely", "[engine][simultaneous]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . bR\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg; // allowSimultaneousMovement=true כברירת מחדל
    kungfu::GameEngine game(board, ruleEngine, cfg);

    auto whiteMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(whiteMove.isAccepted);

    auto blackMove = game.requestMove(kungfu::Position(2, 2), kungfu::Position(2, 1));
    REQUIRE(blackMove.isAccepted); // אין צורך להמתין לתור או לנחיתת הלבן
}

TEST_CASE("Deterministic Simulated Time Flow", "[engine][simultaneous]") {
    std::string boardStr =
        ". . . .\n"
        "wR . . bK\n" // צריח ב-(1,0), מלך שחור ב-(1,3)
        ". . . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg;
    cfg.allowSimultaneousMovement = false;
    cfg.enablePremoves = false;
    kungfu::GameEngine game(board, ruleEngine, cfg);

    SECTION("Enforce single active motion limitation") {
        game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 2));

        auto secondaryMove = game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 1));
        REQUIRE_FALSE(secondaryMove.isAccepted);
        REQUIRE(secondaryMove.reason == "motion_in_progress");
    }

    SECTION("Capturing the enemy King ends the game") {
        game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 3));
        REQUIRE_FALSE(game.isGameOver());

        game.wait(3000);
        REQUIRE(game.isGameOver());

        auto postGameOverMove = game.requestMove(kungfu::Position(1, 3), kungfu::Position(1, 2));
        REQUIRE_FALSE(postGameOverMove.isAccepted);
        REQUIRE(postGameOverMove.reason == "game_over");
    }

    SECTION("Piece Cooldown after Arrival") {
        auto moveRes = game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 1));
        REQUIRE(moveRes.isAccepted);

        game.wait(1000);
        REQUIRE(board->pieceAt(kungfu::Position(1, 1)).has_value());

        auto quickMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
        REQUIRE_FALSE(quickMove.isAccepted);
        REQUIRE(quickMove.reason == "piece_on_cooldown");

        game.wait(1500);
        auto partialWaitMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
        REQUIRE_FALSE(partialWaitMove.isAccepted);
        REQUIRE(partialWaitMove.reason == "piece_on_cooldown");

        game.wait(500);
        auto afterCooldownMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
        REQUIRE(afterCooldownMove.isAccepted);
    }
}

TEST_CASE("Mid-Route Enemy Collision Resolution", "[engine][simultaneous]") {
    std::string boardStr =
        "wR . bR\n"
        ". . .\n"
        ". . .\n"; // צריח לבן ב-(0,0), צריח שחור ב-(0,2)

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res1 = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 2)); // t=0
    REQUIRE(res1.isAccepted);

    game.wait(500); // t=500

    auto res2 = game.requestMove(kungfu::Position(0, 2), kungfu::Position(0, 0)); // t=500
    REQUIRE(res2.isAccepted);

    // חוק מעודכן: השחור (הגיע מאוחר יותר) אוכל את הלבן (הגיע קודם)
    game.wait(1000); // t=1500: ההתנגשות מתרחשת. הלבן (winner) נלכד ומפונה.
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 0)).has_value());

    game.wait(1000); // t=2500: השחור מגיע ליעדו ב-(0,0)
    REQUIRE(board->pieceAt(kungfu::Position(0, 0)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(0, 0)).value()->color() == kungfu::PlayerColor::Black);
}

TEST_CASE("Pawn hasMoved Flag Updates Automatically Upon Arrival", "[engine][simultaneous]") {
    std::string boardStr =
        ". . .\n"
        "wP . .\n"
        ". . .\n"
        ". . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto pawn = board->pieceAt(kungfu::Position(1, 0)).value();
    REQUIRE_FALSE(pawn->hasMoved());

    auto res = game.requestMove(kungfu::Position(1, 0), kungfu::Position(3, 0));
    REQUIRE(res.isAccepted);

    game.wait(1000);
    REQUIRE_FALSE(pawn->hasMoved());

    game.wait(1000);
    REQUIRE(pawn->hasMoved());

    auto secondDoubleStep = game.requestMove(kungfu::Position(3, 0), kungfu::Position(4, 0));
    REQUIRE(secondDoubleStep.isAccepted);
}

TEST_CASE("Knights Do Not Collide Mid-Route", "[engine][simultaneous][knight]") {
    // פרשים נעים בקו אחד שאינו crossing – מוודאים שאין ביטול mid-route
    // לבן ב-(0,0) קופץ ל-(1,2); שחור ב-(3,2) קופץ ל-(2,0)
    // המסלולים אינם crossing, שניהם צריכים לנחות
    std::string boardStr =
        "wN . . .\n"
        ". . . .\n"
        ". . . .\n"
        ". . bN .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto whiteMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(1, 2));
    REQUIRE(whiteMove.isAccepted);

    auto blackMove = game.requestMove(kungfu::Position(3, 2), kungfu::Position(2, 0));
    REQUIRE(blackMove.isAccepted);

    // שניהם צריכים לנחות ולא להיות מבוטלים
    game.wait(2000);
    REQUIRE(board->pieceAt(kungfu::Position(1, 2)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(2, 0)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(1, 2)).value()->color() == kungfu::PlayerColor::White);
    REQUIRE(board->pieceAt(kungfu::Position(2, 0)).value()->color() == kungfu::PlayerColor::Black);
}

TEST_CASE("Knight Captures Friendly Piece on Landing", "[engine][simultaneous][knight]") {
    // פרש לבן ב-(0,0) קופץ ל-(2,1) שם יושב רגלי לבן – הפרש מחסל את הרגלי שלו
    std::string boardStr =
        "wN . .\n"
        ". . .\n"
        ". wP .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res = game.requestMove(kungfu::Position(0, 0), kungfu::Position(2, 1));
    REQUIRE(res.isAccepted);

    // jumpDurationMs=1000, cooldown=2000 – ממתינים מעבר לנחיתה
    game.wait(2000);

    // הפרש נחת על (2,1) והחליף את הרגלי
    REQUIRE(board->pieceAt(kungfu::Position(2, 1)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(2, 1)).value()->type() == kungfu::PieceType::Knight);
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 0)).has_value());
}
