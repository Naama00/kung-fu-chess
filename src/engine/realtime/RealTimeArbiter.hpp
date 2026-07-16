// include/realtime/RealTimeArbiter.hpp
#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <functional>
#include "engine/realtime/Motion.hpp"
#include "engine/realtime/CooldownTracker.hpp"
#include "engine/board/IBoard.hpp"
#include "engine/common/GameConfig.hpp"
#include "engine/common/ArrivalEvent.hpp"
#include "engine/common/MoveResult.hpp"

namespace kungfu {

using PromotionHandler = std::function<PiecePtr(const PiecePtr&, const Position&)>;

class RealTimeArbiter {
public:
    explicit RealTimeArbiter(std::shared_ptr<IBoard> board, GameConfig config = GameConfig{}) noexcept;
    bool hasActiveMotion() const noexcept;
    void startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept;
    std::vector<ArrivalEvent> advanceTime(int ms, int& currentTimeMs, PromotionHandler promoteCallback = nullptr) noexcept;
    bool isOnCooldown(const PiecePtr& piece, int currentTimeMs) const noexcept;
    bool isPieceMoving(const PiecePtr& piece) const noexcept;
    std::optional<Motion> getMotionForPiece(const ConstPiecePtr& piece) const noexcept;
    std::optional<PiecePtr> getPieceInTransitAt(const Position& pos) const noexcept;
    size_t cooldownEntryCount() const noexcept { return cooldownTracker_.entryCount(); }
    // מתודות הניהול הפיזיקלי של תנועות הכלים
    bool isPieceBusy(const PiecePtr& piece, int currentTimeMs) const noexcept;
    MoveResult executeMove(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs) noexcept;
    float getCooldownProgress(const PiecePtr& piece, int currentTimeMs) const noexcept;
    const std::vector<Motion>& activeMotions() const noexcept { return activeMotions_; }
private:
    std::shared_ptr<IBoard> board_;
    std::vector<Motion> activeMotions_;
    CooldownTracker cooldownTracker_;
    GameConfig config_;
};

}  // namespace kungfu