#pragma once

#include <memory>
#include "engine/board/IBoard.hpp"
#include "engine/common/Position.hpp"
#include "engine/board/Piece.hpp" // הוספת ייבוא של Piece
#include <string>

namespace kungfu {

struct MoveValidation {
    bool isValid;
    std::string reason;
};

class RuleEngine {
public:
    explicit RuleEngine(std::shared_ptr<IBoard> board) noexcept;

    // בודק חוקיות מלאה של מהלך עבור הלוח הנוכחי ומחזיר שגיאות מובנות
    MoveValidation validateMove(const Position& from, const Position& to) const;

    // מתודה חדשה: סימולציה ואימות של מהלך היפותטי (למשל פרה-מוב)
    MoveValidation validateHypotheticalMove(const PiecePtr& piece, const Position& from, const Position& to) const;

private:
    std::shared_ptr<IBoard> board_;
};

}  // namespace kungfu