// engine/analysis/PositionEvaluator.hpp
#pragma once
#include "engine/analysis/IPositionEvaluator.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

// Forward declarations to avoid heavy circular dependencies
namespace view {
    struct GameSnapshot;
}
class IBoard;
class RealTimeArbiter;

class PositionEvaluator : public IPositionEvaluator {
public:
    PositionEvaluator() = default;
    ~PositionEvaluator() override = default;

    static int getPieceValue(PieceType type) noexcept;

    int evaluate(const view::GameSnapshot& snapshot, PlayerColor evaluatingPlayer) const noexcept override;

    static int evaluateBalance(const IBoard& board, const RealTimeArbiter& arbiter) noexcept;
};

} // namespace kungfu