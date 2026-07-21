// unit/engine/PositionEvaluatorAndThreatTests.cpp
#include <catch2/catch_test_macros.hpp>
#include "engine/io/BoardParser.hpp"
#include "engine/analysis/ThreatAnalyzer.hpp"
#include "engine/analysis/PositionEvaluator.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "engine/core/GameEngine.hpp"

namespace {

kungfu::view::GameSnapshot buildSnapshotFromBoard(const std::shared_ptr<kungfu::Board>& board) {
    kungfu::GameConfig config;
    kungfu::RealTimeArbiter arbiter(board, config);
    return kungfu::view::SnapshotBuilder::build(*board, arbiter, 0, false, std::nullopt);
}

} // namespace

TEST_CASE("ThreatAnalyzer correctly detects threats in GameSnapshot", "[analysis][threat]") {
    std::string boardSetup =
        "wR . . bK\n"
        ". . . .\n"
        ". . . .\n"
        ". . . .\n";

    auto board = kungfu::BoardParser::parse(boardSetup);
    REQUIRE(board != nullptr);

    auto snapshot = buildSnapshotFromBoard(board);
    kungfu::ThreatAnalyzer threatAnalyzer;

    SECTION("White Rook threatens the square it can move to") {
        bool isKingThreatened = threatAnalyzer.isThreatened(snapshot, kungfu::Position(0, 3), kungfu::PlayerColor::Black);
        REQUIRE(isKingThreatened);

        auto threateners = threatAnalyzer.getThreateningPieces(snapshot, kungfu::Position(0, 3), kungfu::PlayerColor::Black);
        REQUIRE(threateners.size() == 1);
        REQUIRE(threateners[0].type == kungfu::PieceType::Rook);
        REQUIRE(threateners[0].color == kungfu::PlayerColor::White);
    }

    SECTION("Rook is not threatened by King") {
        bool isRookThreatened = threatAnalyzer.isThreatened(snapshot, kungfu::Position(0, 0), kungfu::PlayerColor::White);
        REQUIRE_FALSE(isRookThreatened);
    }
}

TEST_CASE("PositionEvaluator correctly evaluates material and threats", "[analysis][evaluation]") {
    std::string boardSetup =
        "wQ . . bK\n"
        ". . . .\n"
        ". . . .\n"
        ". . . .\n";

    auto board = kungfu::BoardParser::parse(boardSetup);
    REQUIRE(board != nullptr);

    auto snapshot = buildSnapshotFromBoard(board);
    kungfu::PositionEvaluator evaluator;

    int whiteScore = evaluator.evaluate(snapshot, kungfu::PlayerColor::White);
    int blackScore = evaluator.evaluate(snapshot, kungfu::PlayerColor::Black);

    // White should have a positive score as they have a Queen and threaten Black's King
    REQUIRE(whiteScore > 0);
    REQUIRE(blackScore < 0);
}