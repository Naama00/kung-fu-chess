#pragma once

#include <memory>
#include <optional>
#include <string>
#include "core/input/BoardMapper.hpp"
#include "core/common/Position.hpp"
#include "core/input/InputConfig.hpp" 

namespace kungfu {

class IGameEngine;

struct ControllerResult {
    bool actionTaken;
    std::string description;
};

class Controller {
public:
    // שימוש בקבוע כערך ברירת מחדל
    Controller(std::shared_ptr<IGameEngine> engine, int cellSize = InputConfig::kDefaultCellSize);

    ControllerResult click(int x, int y);
    std::optional<Position> selectedPosition() const noexcept;
    void clearSelection() noexcept;
    void setCellSize(int cellSize) noexcept { mapper_.setCellSize(cellSize); }
private:
    std::shared_ptr<IGameEngine> engine_;
    BoardMapper mapper_;
    std::optional<Position> selectedPosition_;
};

}  // namespace kungfu