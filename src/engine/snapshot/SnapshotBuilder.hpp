// engine/snapshot/SnapshotBuilder.hpp
#pragma once

#include <optional>
#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/board/IBoard.hpp"
#include "engine/realtime/RealTimeArbiter.hpp"

namespace kungfu {
namespace view {

class SnapshotBuilder {
public:
    // Calculates a pure logical representations without screen metrics dependency
    static GameSnapshot build(
        const IBoard& board,
        const RealTimeArbiter& arbiter,
        int currentTimeMs,
        bool gameOver,
        std::optional<Position> selectedCell
    ) noexcept;
};

}  // namespace view
}  // namespace kungfu  