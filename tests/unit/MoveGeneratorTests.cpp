#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <vector>

#include "engine/analysis/MoveGenerator.hpp"

namespace kungfu {
bool operator==(const IMoveGenerator::MoveCandidate& lhs, const IMoveGenerator::MoveCandidate& rhs) {
    return lhs.from == rhs.from && lhs.to == rhs.to;
}
} // namespace kungfu
#include "engine/board/Board.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace {

using kungfu::Board;
using kungfu::BoardPtr;
using kungfu::Position;
using kungfu::PlayerColor;
using kungfu::RuleEngine;
using kungfu::view::GameSnapshot;
using kungfu::view::PieceSnapshot;

GameSnapshot snapshotFromBoard(const BoardPtr& board) {
    GameSnapshot snapshot;
    snapshot.boardCols = board->cols();
    snapshot.boardRows = board->rows();
    snapshot.isGameOver = false;
    snapshot.selectedCell.reset();

    const auto pieces = board->pieces();
    snapshot.pieces.reserve(pieces.size());
    for (const auto& piece : pieces) {
        if (!piece) {
            continue;
        }

        PieceSnapshot pieceSnapshot;
        pieceSnapshot.type = piece->type();
        pieceSnapshot.color = piece->color();
        pieceSnapshot.logicalPosition = piece->position();
        pieceSnapshot.state = piece->state();
        pieceSnapshot.hasMoved = piece->hasMoved();
        snapshot.pieces.push_back(pieceSnapshot);
    }
    return snapshot;
}

std::vector<Position> expectedMovesForPiece(const BoardPtr& board, const Position& from) {
    RuleEngine engine(board);
    std::vector<Position> result;
    for (int row = 0; row < board->rows(); ++row) {
        for (int col = 0; col < board->cols(); ++col) {
            Position to(row, col);
            if (engine.validateMove(from, to).isValid) {
                result.push_back(to);
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const Position& lhs, const Position& rhs) {
        if (lhs.row() != rhs.row()) {
            return lhs.row() < rhs.row();
        }
        return lhs.col() < rhs.col();
    });
    return result;
}

std::vector<Position> generatedDestinations(const std::vector<kungfu::IMoveGenerator::MoveCandidate>& moves) {
    std::vector<Position> result;
    result.reserve(moves.size());
    for (const auto& move : moves) {
        result.push_back(move.to);
    }

    std::sort(result.begin(), result.end(), [](const Position& lhs, const Position& rhs) {
        if (lhs.row() != rhs.row()) {
            return lhs.row() < rhs.row();
        }
        return lhs.col() < rhs.col();
    });
    return result;
}

std::vector<kungfu::IMoveGenerator::MoveCandidate> expectedMovesForPlayer(const BoardPtr& board, PlayerColor playerColor) {
    std::vector<kungfu::IMoveGenerator::MoveCandidate> result;
    RuleEngine engine(board);

    for (const auto& piece : board->pieces()) {
        if (!piece || piece->color() != playerColor) {
            continue;
        }

        for (int row = 0; row < board->rows(); ++row) {
            for (int col = 0; col < board->cols(); ++col) {
                Position target(row, col);
                if (engine.validateMove(piece->position(), target).isValid) {
                    result.push_back({piece->position(), target});
                }
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const kungfu::IMoveGenerator::MoveCandidate& lhs, const kungfu::IMoveGenerator::MoveCandidate& rhs) {
        if (lhs.from.row() != rhs.from.row()) {
            return lhs.from.row() < rhs.from.row();
        }
        if (lhs.from.col() != rhs.from.col()) {
            return lhs.from.col() < rhs.from.col();
        }
        if (lhs.to.row() != rhs.to.row()) {
            return lhs.to.row() < rhs.to.row();
        }
        return lhs.to.col() < rhs.to.col();
    });

    return result;
}

} // namespace

TEST_CASE("MoveGenerator mirrors RuleEngine for rook and knight rules", "[analysis][moves]") {
    std::string boardStr =
        ". . . . .\n"
        ". wR . bP .\n"
        ". . . . .\n"
        ". wP . . .\n"
        ". . . . .\n";

    const auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);

    const GameSnapshot snapshot = snapshotFromBoard(board);
    kungfu::MoveGenerator generator;

    const Position rookFrom(1, 1);
    const auto expected = expectedMovesForPiece(board, rookFrom);
    const auto generated = generatedDestinations(generator.generateForPiece(snapshot, rookFrom));
    REQUIRE(generated == expected);

    const Position knightFrom(0, 0);
    const auto knightBoard = std::make_shared<Board>(3, 3);
    std::string knightBoardStr =
        "wN wP .\n"
        "wP wP .\n"
        ". . .\n";
    const auto knightParsed = kungfu::BoardParser::parse(knightBoardStr);
    REQUIRE(knightParsed != nullptr);
    const GameSnapshot knightSnapshot = snapshotFromBoard(knightParsed);
    const auto knightExpected = expectedMovesForPiece(knightParsed, knightFrom);
    const auto knightGenerated = generatedDestinations(generator.generateForPiece(knightSnapshot, knightFrom));
    REQUIRE(knightGenerated == knightExpected);
}

TEST_CASE("MoveGenerator mirrors RuleEngine for king, bishop, queen, and pawn scenarios", "[analysis][moves]") {
    SECTION("King and bishop/queen movement") {
        std::string boardStr =
            "wK . . . .\n"
            ". . . . .\n"
            ". . wB . .\n"
            ". . . . wQ\n";

        const auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        const GameSnapshot snapshot = snapshotFromBoard(board);
        kungfu::MoveGenerator generator;

        const Position kingFrom(0, 0);
        const auto kingExpected = expectedMovesForPiece(board, kingFrom);
        const auto kingGenerated = generatedDestinations(generator.generateForPiece(snapshot, kingFrom));
        REQUIRE(kingGenerated == kingExpected);

        const Position bishopFrom(2, 2);
        const auto bishopExpected = expectedMovesForPiece(board, bishopFrom);
        const auto bishopGenerated = generatedDestinations(generator.generateForPiece(snapshot, bishopFrom));
        REQUIRE(bishopGenerated == bishopExpected);

        const Position queenFrom(3, 4);
        const auto queenExpected = expectedMovesForPiece(board, queenFrom);
        const auto queenGenerated = generatedDestinations(generator.generateForPiece(snapshot, queenFrom));
        REQUIRE(queenGenerated == queenExpected);
    }

    SECTION("Pawn movement and double-step state") {
        std::string boardStr =
            ". . . . . . . .\n"
            "bP . bP . . . . .\n"
            ". . wR . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n";

        const auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        const GameSnapshot snapshot = snapshotFromBoard(board);
        kungfu::MoveGenerator generator;

        const Position pawnFrom(1, 0);
        const auto pawnExpected = expectedMovesForPiece(board, pawnFrom);
        const auto pawnGenerated = generatedDestinations(generator.generateForPiece(snapshot, pawnFrom));
        REQUIRE(pawnGenerated == pawnExpected);

        auto movedBoard = std::make_shared<Board>(8, 8);
        const auto freshBoard = kungfu::BoardParser::parse(
            ". . .\n"
            ". . .\n"
            ". . .\n"
            ". . .\n"
            "bP . .\n"
            ". . .\n"
            ". . .\n");
        REQUIRE(freshBoard != nullptr);
        auto pawn = freshBoard->pieceAt(Position(4, 0)).value();
        pawn->markMoved();
        const auto movedSnapshot = snapshotFromBoard(freshBoard);
        const Position freshFrom(4, 0);
        const auto freshExpected = expectedMovesForPiece(freshBoard, freshFrom);
        const auto freshGenerated = generatedDestinations(generator.generateForPiece(movedSnapshot, freshFrom));
        REQUIRE(freshGenerated == freshExpected);
    }
}

TEST_CASE("MoveGenerator returns moves for all pieces of a player", "[analysis][moves]") {
    std::string boardStr =
        "wR . .\n"
        ". wN .\n"
        ". . .\n";

    const auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    const GameSnapshot snapshot = snapshotFromBoard(board);
    kungfu::MoveGenerator generator;

    const auto generated = generator.generateForPlayer(snapshot, PlayerColor::White);
    const auto expected = expectedMovesForPlayer(board, PlayerColor::White);
    REQUIRE(generated.size() == expected.size());
    REQUIRE(generated == expected);
}
