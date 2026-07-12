#include <catch2/catch_test_macros.hpp>
#include "board/Board.hpp"
#include "io/BoardParser.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"

TEST_CASE("Advanced Real-Time Jump Mechanics (Airborne and Captures)", "[engine][jump]") {
    SECTION("Jump and Normal Landing") {
        std::string boardStr =
            "wR . .\n"
            ". . .\n"
            ". . .\n";

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
        kungfu::GameEngine game(board, ruleEngine);

        auto res = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 0));
        REQUIRE(res.isAccepted);
        REQUIRE(res.reason == "jump_started");

        auto rook = board->pieceAt(kungfu::Position(0, 0)).value();
        REQUIRE(rook->state() == kungfu::PieceState::Airborne);

        game.wait(500);
        REQUIRE(rook->state() == kungfu::PieceState::Airborne);

        game.wait(500);
        REQUIRE(rook->state() == kungfu::PieceState::Idle);
    }

    SECTION("Airborne Piece Captures Arriving Enemy") {
        std::string boardStr =
            "wR . bR\n"
            ". . .\n"
            ". . .\n"; // צריח לבן ב-(0,0), צריח שחור ב-(0,2)

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
        kungfu::GameEngine game(board, ruleEngine);

        game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 0));
        game.wait(200);

        board->removePiece(kungfu::Position(0, 2));
        auto blackRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::Black, kungfu::Position(0, 1));
        board->placePiece(blackRook, kungfu::Position(0, 1));
        // (הסעיף הזה מכין את הסצנה; הלוגיקה המלאה מכוסה ב-Detailed Airborne Capture Sync Test)
    }

    SECTION("Detailed Airborne Capture Sync Test") {
        std::string boardStr =
            "wR bR .\n"
            ". . .\n"
            ". . .\n"; // לבן ב-(0,0), שחור ב-(0,1)

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
        kungfu::GameEngine game(board, ruleEngine);

        game.requestMove(kungfu::Position(0, 1), kungfu::Position(0, 0));

        game.wait(200);
        game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 0));

        game.wait(800);

        REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 1)).has_value());

        auto whiteRook = board->pieceAt(kungfu::Position(0, 0)).value();
        REQUIRE(whiteRook->state() == kungfu::PieceState::Airborne);

        game.wait(200);
        REQUIRE(whiteRook->state() == kungfu::PieceState::Idle);
    }
}