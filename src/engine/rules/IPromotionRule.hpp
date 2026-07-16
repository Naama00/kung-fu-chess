#pragma once

#include "engine/board/Piece.hpp"
#include "engine/board/IBoard.hpp"
#include "engine/common/Position.hpp"

namespace kungfu {

class IPromotionRule {
public:
    virtual ~IPromotionRule() = default;

    // בודקת האם הכלי הנתון זכאי להכתרה במיקום היעד, ובמידת הצורך
    // מבצעת את ההחלפה בפועל בלוח ומחזירה את הכלי החדש.
    // אם אין הכתרה - מחזירה את אותו piece שהתקבל ללא שינוי.
    virtual PiecePtr maybePromote(const PiecePtr& piece, const Position& to, IBoard& board) const = 0;
};

}  // namespace kungfu