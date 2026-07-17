#include <catch2/catch_test_macros.hpp>
#include "players/human/HumanPlayer.hpp"
#include "engine/core/IGameEngine.hpp"
#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace {

class MockGameEngine : public kungfu::IGameEngine {
public:
    explicit MockGameEngine(int rows = 8, int cols = 8) : rows_(rows), cols_(cols) {}

    void setHasPiece(const kungfu::Position& pos, bool hasPiece, kungfu::PlayerColor color = kungfu::PlayerColor::White) {
        if (hasPiece) {
            if (std::find(occupiedPositions_.begin(), occupiedPositions_.end(), pos) == occupiedPositions_.end()) {
                occupiedPositions_.push_back(pos);
                pieceColors_.push_back({pos, color});
            }
        } else {
            occupiedPositions_.erase(std::remove(occupiedPositions_.begin(), occupiedPositions_.end(), pos), occupiedPositions_.end());
            pieceColors_.erase(std::remove_if(pieceColors_.begin(), pieceColors_.end(), [&](const auto& pair) {
                return pair.first == pos;
            }), pieceColors_.end());
        }
    }

    void setMoveResponse(bool accept, const std::string& reason) {
        nextMoveResult_ = {accept, reason};
    }

    kungfu::MoveResult requestMove(const kungfu::Position& from, const kungfu::Position& to) override {
        lastMoveFrom_ = from;
        lastMoveTo_ = to;
        ++moveCount_;
        return nextMoveResult_;
    }

    std::vector<kungfu::ActionResult> processActionRequests(const std::vector<kungfu::ActionRequest>& requests) override {
        processedRequests_ = requests;
        std::vector<kungfu::ActionResult> results;
        results.reserve(requests.size());
        for (const auto& request : requests) {
            results.emplace_back(request.requestId, nextMoveResult_.isAccepted ? kungfu::ActionStatus::Accepted : kungfu::ActionStatus::Rejected);
        }
        return results;
    }

    bool hasPieceAt(const kungfu::Position& pos) const override {
        return std::find(occupiedPositions_.begin(), occupiedPositions_.end(), pos) != occupiedPositions_.end();
    }

    std::optional<kungfu::PlayerColor> getPieceColorAt(const kungfu::Position& pos) const override {
        for (const auto& pair : pieceColors_) {
            if (pair.first == pos) {
                return pair.second;
            }
        }
        return std::nullopt;
    }

    int getBoardRows() const override { return rows_; }
    int getBoardCols() const override { return cols_; }

    int getMoveCount() const { return moveCount_; }
    const std::vector<kungfu::ActionRequest>& processedRequests() const { return processedRequests_; }

private:
    int rows_;
    int cols_;
    std::vector<kungfu::Position> occupiedPositions_;
    std::vector<std::pair<kungfu::Position, kungfu::PlayerColor>> pieceColors_;
    kungfu::MoveResult nextMoveResult_ = {true, "ok"};
    std::vector<kungfu::ActionRequest> processedRequests_;
    int moveCount_ = 0;
    kungfu::Position lastMoveFrom_;
    kungfu::Position lastMoveTo_;
};

} // namespace

TEST_CASE("HumanPlayer wraps controller clicks into ActionRequest objects", "[player]") {
    auto mockEngine = std::make_shared<MockGameEngine>(8, 8);
    mockEngine->setHasPiece(kungfu::Position(1, 1), true, kungfu::PlayerColor::White);

    kungfu::HumanPlayer player(mockEngine, 100);

    auto firstResult = player.handleClick(150, 150);
    REQUIRE(firstResult.actionTaken);
    REQUIRE(firstResult.description == "Piece selected");

    auto secondResult = player.handleClick(250, 250);
    REQUIRE(secondResult.actionTaken);
    REQUIRE(secondResult.description == "Move requested: ok");

    auto requests = player.decideActions(kungfu::view::GameSnapshot{});
    REQUIRE(requests.size() == 1);
    REQUIRE(requests.front().action.from == kungfu::Position(1, 1));
    REQUIRE(requests.front().action.to == kungfu::Position(2, 2));
    REQUIRE(requests.front().playerColor == kungfu::PlayerColor::White);
}

TEST_CASE("HumanPlayer does not queue a request when selection is cleared", "[player]") {
    auto mockEngine = std::make_shared<MockGameEngine>(8, 8);
    mockEngine->setHasPiece(kungfu::Position(1, 1), true, kungfu::PlayerColor::White);

    kungfu::HumanPlayer player(mockEngine, 100);

    auto firstResult = player.handleClick(150, 150);
    REQUIRE(firstResult.actionTaken);
    REQUIRE(firstResult.description == "Piece selected");

    auto secondResult = player.handleClick(150, 150);
    REQUIRE(secondResult.actionTaken);
    REQUIRE(secondResult.description == "Selection cleared");

    auto requests = player.decideActions(kungfu::view::GameSnapshot{});
    REQUIRE(requests.empty());
}
