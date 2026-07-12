#include <catch2/catch_test_macros.hpp>
#include "board/Board.hpp"
#include "io/BoardParser.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"

TEST_CASE("Turn-Based Mode Enforcement", "[engine][turnbased]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . bR\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg;
    cfg.allowSimultaneousMovement = false;
    cfg.enablePremoves = false;
    kungfu::GameEngine game(board, ruleEngine, cfg);

    SECTION("White moves first by default") {
        REQUIRE(game.currentTurn() == kungfu::PlayerColor::White);
    }

    SECTION("Black cannot move out of turn") {
        auto result = game.requestMove(kungfu::Position(2, 2), kungfu::Position(2, 1));
        REQUIRE_FALSE(result.isAccepted);
        REQUIRE(result.reason == "not_your_turn");
    }

    SECTION("Successful move passes the turn to the opponent") {
        auto result = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
        REQUIRE(result.isAccepted);
        REQUIRE(game.currentTurn() == kungfu::PlayerColor::Black);
    }

    SECTION("Opponent must wait for the piece to arrive before moving") {
        game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1)); // 1000ms
        auto blackAttempt = game.requestMove(kungfu::Position(2, 2), kungfu::Position(2, 1));
        REQUIRE_FALSE(blackAttempt.isAccepted);
        REQUIRE(blackAttempt.reason == "motion_in_progress");

        game.wait(1000);
        auto blackAttempt2 = game.requestMove(kungfu::Position(2, 2), kungfu::Position(2, 1));
        REQUIRE(blackAttempt2.isAccepted);
    }

    SECTION("A rejected move does not pass the turn") {
        auto illegalMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        REQUIRE_FALSE(illegalMove.isAccepted);
        REQUIRE(game.currentTurn() == kungfu::PlayerColor::White);
    }
}