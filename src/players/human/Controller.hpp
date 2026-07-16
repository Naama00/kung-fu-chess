#pragma once

#include <memory>
#include <optional>
#include <string>
#include "players/human/BoardMapper.hpp"
#include "engine/common/Position.hpp"
#include "engine/common/Enums.hpp"
#include "players/human/InputConfig.hpp" 

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
    std::optional<PlayerColor> selectedColor_;
};

}  // namespace kungfu