#include "players/human/BoardMapper.hpp"

namespace kungfu {

BoardMapper::BoardMapper(int cellSize) : cellSize_(cellSize) {}

std::optional<Position> BoardMapper::pixelToCell(int x, int y, int rows, int cols) const noexcept {
    if (x < 0 || y < 0) {
        return std::nullopt;
    }

    int col = x / cellSize_;
    int row = y / cellSize_;

    if (row >= 0 && row < rows && col >= 0 && col < cols) {
        return Position(row, col);
    }

    return std::nullopt;
}

}  // namespace kungfu