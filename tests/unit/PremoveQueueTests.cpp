#include <catch2/catch_test_macros.hpp>
#include "engine/PremoveQueue.hpp"
#include "board/Piece.hpp"

TEST_CASE("PremoveQueue Registration and Update", "[engine][premove]") {
    kungfu::PremoveQueue queue;
    auto piece = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));

    SECTION("Fresh queue is empty") {
        REQUIRE(queue.empty());
    }

    SECTION("Registering a premove adds an entry") {
        queue.registerOrUpdate(piece, kungfu::Position(0, 1));
        REQUIRE(queue.size() == 1);
    }

    SECTION("Registering again for the same piece updates, not duplicates") {
        queue.registerOrUpdate(piece, kungfu::Position(0, 1));
        queue.registerOrUpdate(piece, kungfu::Position(0, 2));
        REQUIRE(queue.size() == 1);
    }
}

TEST_CASE("PremoveQueue Processing", "[engine][premove]") {
    kungfu::PremoveQueue queue;
    auto piece = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
    queue.registerOrUpdate(piece, kungfu::Position(0, 2));

    SECTION("Busy piece is not executed and remains queued") {
        int executeCallCount = 0;
        queue.processReady(
            [](const kungfu::PiecePtr&) { return true; },
            [&](const kungfu::Position&, const kungfu::Position&) -> kungfu::MoveResult {
                executeCallCount++;
                return {true, "ok"};
            }
        );

        REQUIRE(executeCallCount == 0);
        REQUIRE_FALSE(queue.empty());
    }

    SECTION("Free piece is executed and removed from the queue") {
        kungfu::Position calledFrom(0, 0), calledTo(0, 0);
        int executeCallCount = 0;

        queue.processReady(
            [](const kungfu::PiecePtr&) { return false; },
            [&](const kungfu::Position& from, const kungfu::Position& to) -> kungfu::MoveResult {
                calledFrom = from;
                calledTo = to;
                executeCallCount++;
                return {true, "ok"};
            }
        );

        REQUIRE(executeCallCount == 1);
        REQUIRE(calledTo == kungfu::Position(0, 2));
        REQUIRE(queue.empty());
    }

    SECTION("Captured piece is dropped from the queue without executing") {
        piece->setState(kungfu::PieceState::Captured);
        int executeCallCount = 0;

        queue.processReady(
            [](const kungfu::PiecePtr&) { return false; },
            [&](const kungfu::Position&, const kungfu::Position&) -> kungfu::MoveResult {
                executeCallCount++;
                return {true, "ok"};
            }
        );

        REQUIRE(executeCallCount == 0);
        REQUIRE(queue.empty());
    }

    SECTION("Executor receives the piece's current position, not the original registration position") {
        piece->setPosition(kungfu::Position(0, 1));

        kungfu::Position calledFrom(-1, -1);
        queue.processReady(
            [](const kungfu::PiecePtr&) { return false; },
            [&](const kungfu::Position& from, const kungfu::Position&) -> kungfu::MoveResult {
                calledFrom = from;
                return {true, "ok"};
            }
        );

        REQUIRE(calledFrom == kungfu::Position(0, 1));
    }
}