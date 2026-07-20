#include "players/human/Controller.hpp"
#include "engine/core/IGameEngine.hpp"

namespace kungfu {

Controller::Controller(std::shared_ptr<IGameEngine> engine, int cellSize)
    : engine_(std::move(engine)), mapper_(cellSize), selectedPosition_(std::nullopt) {}

ControllerResult Controller::click(int x, int y) {
    ControllerResult result{false, "", std::nullopt, std::nullopt, std::nullopt};

    if (!engine_) {
        result.description = "Engine reference is null";
        return result;
    }

    int rows = engine_->getBoardRows();
    int cols = engine_->getBoardCols();
    auto cellOpt = mapper_.pixelToCell(x, y, rows, cols);

    // 1. Click outside the board boundaries — ignore completely
    if (!cellOpt.has_value()) {
        result.description = "Click outside board ignored";
        return result;
    }

    Position targetCell = cellOpt.value();

    // 2. Click inside the board when a piece is already selected
    if (selectedPosition_.has_value()) {
        Position from = selectedPosition_.value();

        auto currentColorAtFrom = engine_->getPieceColorAt(from);
        bool selectionStillValid = currentColorAtFrom.has_value() &&
                                    selectedColor_.has_value() &&
                                    currentColorAtFrom.value() == selectedColor_.value();

        if (!selectionStillValid) {
            clearSelection();
            if (engine_->hasPieceAt(targetCell)) {
                selectedPosition_ = targetCell;
                selectedColor_ = engine_->getPieceColorAt(targetCell);
                result.actionTaken = true;
                result.description = "Stale selection cleared; new piece selected";
                return result;
            }
            result.description = "Stale selection cleared";
            return result;
        }

        if (from == targetCell) {
            clearSelection();
            result.actionTaken = true;
            result.description = "Selection cleared";
            return result;
        }

        // First attempt: send the movement request to the game engine.
        // If the square is physically empty on the board (even if another piece is on its way), the engine will accept the move!
        auto moveResult = engine_->requestMove(from, targetCell);
        
        if (moveResult.isAccepted) {
            auto selectedColor = selectedColor_;
            clearSelection();

            result.actionTaken = true;
            result.from = from;
            result.to = targetCell;
            result.playerColor = selectedColor;
            result.description = "Move requested: " + moveResult.reason;
            return result;
        }

        // Second attempt: if the move is rejected, check whether the user tried to switch selection to another friendly piece
        if (engine_->hasPieceAt(targetCell)) {
            auto targetColor = engine_->getPieceColorAt(targetCell);
            if (targetColor.has_value() && targetColor.value() == selectedColor_.value()) {
                selectedPosition_ = targetCell;
                selectedColor_ = targetColor;
                result.actionTaken = true;
                result.description = "Piece selection replaced";
                return result;
            }
        }

        // If the move is rejected and it was not a legal selection switch, clear the selection and return the rejection reason
        clearSelection();
        result.actionTaken = true;
        result.description = "Move rejected: " + moveResult.reason;
        return result;
    }

    // 3. Click inside the board without a piece currently selected (first click)
    if (engine_->hasPieceAt(targetCell)) {
        selectedPosition_ = targetCell;
        selectedColor_ = engine_->getPieceColorAt(targetCell);
        result.actionTaken = true;
        result.description = "Piece selected";
        return result;
    }

    result.description = "Click on empty cell ignored";
    return result;
}

std::optional<Position> Controller::selectedPosition() const noexcept {
    return selectedPosition_;
}

void Controller::clearSelection() noexcept {
    selectedPosition_ = std::nullopt;
    selectedColor_ = std::nullopt;
}

}  // namespace kungfu