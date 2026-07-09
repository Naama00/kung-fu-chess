#include <catch2/catch_test_macros.hpp>
#include "common/Position.hpp"
#include "pieces/Piece.hpp"
#include "board/Board.hpp"

TEST_CASE("Position Model Behavior", "[model]") {
    kungfu::Position p1(2, 3);
    kungfu::Position p2(2, 3);
    kungfu::Position p3(4, 5);

    SECTION("Equality and inequality operators") {
        REQUIRE(p1 == p2);
        REQUIRE(p1 != p3);
    }

    SECTION("Coordinate getters") {
        REQUIRE(p1.row() == 2);
        REQUIRE(p1.col() == 3);
    }
}

TEST_CASE("Piece Model Behavior", "[model]") {
    auto piece = std::make_shared<kungfu::Piece>(kungfu::PieceType::King, kungfu::PlayerColor::White, kungfu::Position(0, 0));

    SECTION("Attributes and initial state") {
        REQUIRE(piece->type() == kungfu::PieceType::King);
        REQUIRE(piece->color() == kungfu::PlayerColor::White);
        REQUIRE(piece->position() == kungfu::Position(0, 0));
        REQUIRE(piece->state() == kungfu::PieceState::Idle);
    }

    SECTION("Setters modify state correctly") {
        piece->setPosition(kungfu::Position(1, 1));
        REQUIRE(piece->position() == kungfu::Position(1, 1));

        piece->setState(kungfu::PieceState::Moving);
        REQUIRE(piece->state() == kungfu::PieceState::Moving);
    }
}

TEST_CASE("Board Management and Mutators", "[model]") {
    kungfu::Board board(8, 8);
    auto whiteKing = std::make_shared<kungfu::Piece>(kungfu::PieceType::King, kungfu::PlayerColor::White, kungfu::Position(0, 0));
    auto blackKing = std::make_shared<kungfu::Piece>(kungfu::PieceType::King, kungfu::PlayerColor::Black, kungfu::Position(1, 1));

    SECTION("Placing and retrieving pieces") {
        REQUIRE(board.placePiece(whiteKing, kungfu::Position(0, 0)));
        auto retrieved = board.pieceAt(kungfu::Position(0, 0));
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved.value() == whiteKing);
    }

    SECTION("Prevent duplicate occupancy on the same cell") {
        REQUIRE(board.placePiece(whiteKing, kungfu::Position(0, 0)));
        // הנחת כלי נוסף על משבצת תפוסה צריכה להיכשל
        REQUIRE_FALSE(board.placePiece(blackKing, kungfu::Position(0, 0)));
    }

    SECTION("Moving pieces updates coordinates logically") {
        REQUIRE(board.placePiece(whiteKing, kungfu::Position(0, 0)));
        REQUIRE(board.movePiece(kungfu::Position(0, 0), kungfu::Position(1, 1)));
        
        REQUIRE_FALSE(board.pieceAt(kungfu::Position(0, 0)).has_value());
        REQUIRE(board.pieceAt(kungfu::Position(1, 1)).has_value());
        REQUIRE(board.pieceAt(kungfu::Position(1, 1)).value() == whiteKing);
    }

    SECTION("Removing pieces from the board") {
        REQUIRE(board.placePiece(whiteKing, kungfu::Position(0, 0)));
        REQUIRE(board.removePiece(kungfu::Position(0, 0)));
        REQUIRE_FALSE(board.pieceAt(kungfu::Position(0, 0)).has_value());
        
        // ניסיון הסרה ממשבצת ריקה צריך להיכשל
        REQUIRE_FALSE(board.removePiece(kungfu::Position(0, 0)));
    }
}