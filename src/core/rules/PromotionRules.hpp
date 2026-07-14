#pragma once

#include "core/rules/IPromotionRule.hpp"

namespace kungfu {

// חוק הכתרה קלאסי: רגלי שמגיע לשורה האחרונה (יחסית לכיוון התקדמותו ולגובה הלוח בפועל)
// הופך אוטומטית למלכה.
class ChessPromotionRule : public IPromotionRule {
public:
    PiecePtr maybePromote(const PiecePtr& piece, const Position& to, IBoard& board) const override;
};

}  // namespace kungfu