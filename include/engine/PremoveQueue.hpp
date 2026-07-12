#pragma once

#include <vector>
#include <functional>
#include "board/Piece.hpp"
#include "common/Position.hpp"
#include "engine/IGameEngine.hpp"

namespace kungfu {

using PieceBusyPredicate = std::function<bool(const PiecePtr&)>;
using MoveExecutor = std::function<MoveResult(const Position& from, const Position& to)>;

class PremoveQueue {
public:
    // רושמת premove חדש, או מעדכנת premove קיים לאותו כלי.
    // רק היעד (to) נשמר - הביצוע תמיד ישתמש במיקום החי של הכלי כ-from,
    // כי הכלי עשוי לזוז בינתיים (למשל אחרי שהמהלך הקודם שלו נחת).
    void registerOrUpdate(const PiecePtr& piece, const Position& to);

    // מבטלת premove קיים לכלי נתון (אם קיים).
    // נקראת כאשר השחקן מבצע מהלך לא חוקי – הכוונה לדרוס ולבטל את ה-premove.
    void cancel(const PiecePtr& piece) noexcept;

    void processReady(const PieceBusyPredicate& isBusy, const MoveExecutor& execute);

    bool empty() const noexcept { return entries_.empty(); }
    size_t size() const noexcept { return entries_.size(); }

private:
    std::vector<std::pair<PiecePtr, Position>> entries_;
};

}  // namespace kungfu