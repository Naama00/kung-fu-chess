#include <catch2/catch_test_macros.hpp>
#include "core/input/Controller.hpp"
#include "core/engine/IGameEngine.hpp"
#include <vector>
#include <algorithm>

namespace {

class MockGameEngine : public kungfu::IGameEngine {
public:
    MockGameEngine(int rows, int cols) : rows_(rows), cols_(cols) {}

    // שינוי ה-Mock כדי לתמוך בצבעי הכלים
    void setHasPiece(const kungfu::Position& pos, bool hasPiece, kungfu::PlayerColor color = kungfu::PlayerColor::White) {
        if (hasPiece) {
            if (std::find(occupiedPositions_.begin(), occupiedPositions_.end(), pos) == occupiedPositions_.end()) {
                occupiedPositions_.push_back(pos);
                pieceColors_.push_back({pos, color});
            }
        } else {
            auto it = std::find(occupiedPositions_.begin(), occupiedPositions_.end(), pos);
            if (it != occupiedPositions_.end()) {
                occupiedPositions_.erase(it);
            }
            auto it_color = std::remove_if(pieceColors_.begin(), pieceColors_.end(), [&](const auto& pair) {
                return pair.first == pos;
            });
            pieceColors_.erase(it_color, pieceColors_.end());
        }
    }

    void setMoveResponse(bool accept, const std::string& reason) {
        nextMoveResult_ = {accept, reason};
    }

    kungfu::MoveResult requestMove(const kungfu::Position& from, const kungfu::Position& to) override {
        lastMoveFrom_ = from;
        lastMoveTo_ = to;
        moveCount_++;
        return nextMoveResult_;
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
    kungfu::Position getLastMoveFrom() const { return lastMoveFrom_; }
    kungfu::Position getLastMoveTo() const { return lastMoveTo_; }

private:
    int rows_;
    int cols_;
    std::vector<kungfu::Position> occupiedPositions_;
    std::vector<std::pair<kungfu::Position, kungfu::PlayerColor>> pieceColors_;
    kungfu::MoveResult nextMoveResult_ = {true, "ok"};
    
    int moveCount_ = 0;
    kungfu::Position lastMoveFrom_;
    kungfu::Position lastMoveTo_;
};

} // namespace

TEST_CASE("BoardMapper Coordinate Calculations", "[input]") {
    kungfu::BoardMapper mapper(100);

    SECTION("Valid mapping within borders") {
        auto cell = mapper.pixelToCell(50, 150, 8, 8);
        REQUIRE(cell.has_value());
        REQUIRE(cell->row() == 1);
        REQUIRE(cell->col() == 0);
    }

    SECTION("Invalid coordinates and boundary conditions") {
        REQUIRE_FALSE(mapper.pixelToCell(800, 50, 8, 8).has_value());
        REQUIRE_FALSE(mapper.pixelToCell(-10, 50, 8, 8).has_value());
    }
}

TEST_CASE("Controller Selection State and Clicks Flow", "[input]") {
    auto mockEngine = std::make_shared<MockGameEngine>(8, 8);
    kungfu::Controller controller(mockEngine, 100);

    // נמקם כלי לבן במיקום (1, 1)
    mockEngine->setHasPiece(kungfu::Position(1, 1), true, kungfu::PlayerColor::White);

    SECTION("Initial state has no selection") {
        REQUIRE_FALSE(controller.selectedPosition().has_value());
    }

    SECTION("First click on an empty cell is ignored") {
        auto result = controller.click(250, 250);
        REQUIRE_FALSE(result.actionTaken);
    }

    SECTION("First click on a piece selects it") {
        auto result = controller.click(150, 150);
        REQUIRE(result.actionTaken);
        REQUIRE(result.description == "Piece selected");
        REQUIRE(controller.selectedPosition().has_value());
        REQUIRE(controller.selectedPosition().value() == kungfu::Position(1, 1));
    }

    SECTION("Clicking outside the board with selection is ignored (does not cancel selection)") {
        // נבצע בחירה
        controller.click(150, 150);
        REQUIRE(controller.selectedPosition().has_value());

        // קליק מחוץ ללוח
        auto result = controller.click(900, 50);
        REQUIRE_FALSE(result.actionTaken);
        // הבחירה צריכה להישאר פעילה
        REQUIRE(controller.selectedPosition().has_value());
        REQUIRE(controller.selectedPosition().value() == kungfu::Position(1, 1));
    }

    SECTION("Clicking another friendly piece replaces the selection") {
        // נמקם כלי ידידותי נוסף (לבן) ב-(2, 2)
        mockEngine->setHasPiece(kungfu::Position(2, 2), true, kungfu::PlayerColor::White);

        // בחירת הכלי הראשון ב-(1, 1)
        controller.click(150, 150);
        REQUIRE(controller.selectedPosition().value() == kungfu::Position(1, 1));

        // קליק על הכלי השני מחליף את הבחירה אליו
        auto result = controller.click(250, 250);
        REQUIRE(result.actionTaken);
        REQUIRE(result.description == "Piece selection replaced");
        REQUIRE(controller.selectedPosition().value() == kungfu::Position(2, 2));
    }

    SECTION("Second click on empty cell requests a move and clears selection") {
        controller.click(150, 150);
        mockEngine->setMoveResponse(true, "ok");
        auto result = controller.click(250, 250);

        REQUIRE(result.actionTaken);
        REQUIRE(result.description == "Move requested: ok");
        REQUIRE_FALSE(controller.selectedPosition().has_value());
    }
}