#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include "realtime/Motion.hpp"
#include "board/IBoard.hpp"

namespace kungfu {

struct ArrivalEvent {
    Position from;
    Position to;
    PiecePtr piece;
    bool capturedKing;
    bool cancelled; // דגל המציין האם התנועה בוטלה עקב חסימה
};

class RealTimeArbiter {
public:
    explicit RealTimeArbiter(std::shared_ptr<IBoard> board) noexcept;

    bool hasActiveMotion() const noexcept;
    void startMotion(PiecePtr piece, const Position& from, const Position& to, int currentTimeMs, int durationMs) noexcept;
    
    std::vector<ArrivalEvent> advanceTime(int ms, int& currentTimeMs) noexcept;

    bool isOnCooldown(const PiecePtr& piece, int currentTimeMs) const noexcept;
    bool isPieceMoving(const PiecePtr& piece) const noexcept;
    std::optional<Motion> getMotionForPiece(const PiecePtr& piece) const noexcept;

private:
    std::shared_ptr<IBoard> board_;
    std::vector<Motion> activeMotions_; 
    std::unordered_map<const Piece*, int> cooldowns_;

    void handleMidRouteCollisions(std::vector<ArrivalEvent>& events) noexcept;
    bool processSingleArrival(std::vector<Motion>::iterator it, int currentTimeMs, std::vector<ArrivalEvent>& events) noexcept;
    
    // פונקציית עזר לטיפול עצמאי בהכתרת רגלים
    PiecePtr handlePawnPromotion(const PiecePtr& piece, const Position& to) noexcept;
};

}  // namespace kungfu