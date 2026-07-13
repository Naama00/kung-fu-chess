#pragma once

#include <optional>
#include "common/Position.hpp"
#include "input/InputConfig.hpp"   

namespace kungfu {

class BoardMapper {
public:
    // שימוש בקבוע כערך ברירת מחדל
    explicit BoardMapper(int cellSize = InputConfig::kDefaultCellSize);

    std::optional<Position> pixelToCell(int x, int y, int rows, int cols) const noexcept;

    void setCellSize(int cellSize) noexcept { cellSize_ = cellSize; }

private:
    int cellSize_;
};

}  // namespace kungfu